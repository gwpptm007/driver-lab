// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage04.c — stage04 ring / DMA / RX replenishment 教学驱动
 *
 * 【教学目标】
 * 本驱动不模拟完整真实网卡，而是把以下四个核心概念落地成可观察、可调试的教学型实现：
 *   1. descriptor ring（TX/RX 队列槽位，用数组模拟硬件队列）
 *   2. ownership（CPU / device 之间的 buffer 所有权转换）
 *   3. streaming DMA（dma_map_single / dma_unmap_single，每包映射）
 *   4. RX replenishment（poll 处理完一个 slot 后立刻重新补 fresh buffer）
 *
 * 【架构特点】
 * - 没有真实硬件，用 memcpy 模拟 device DMA copy
 * - TX 路径复用 RX ring 的 posted buffer 作为”device 内存”
 * - NAPI poll 按 budget 批量处理 RX done descriptors
 *
 * 【整体数据路径】
 *
 *   userspace send_stage04_frame
 *         │
 *         ▼
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  ndo_start_xmit()           【TX 路径】                      │
 *   │    1. skb_linearize（处理非线性 skb）                        │
 *   │    2. dma_map_single(skb->data, DMA_TO_DEVICE)               │
 *   │    3. stage04_find_posted_rx_slot() 找 POSTED+DEV 的 RX 槽   │
 *   │    4. memcpy(skb->data → RX buffer)  ← 模拟 device DMA copy  │
 *   │    5. RX desc: DONE + CPU owner                              │
 *   │    6. stage04_raise_irq() → napi_schedule()                │
 *   │    7. dma_unmap_single(TX skb)                              │
 *   │    8. dev_consume_skb_any()                                 │
 *   └─────────────────────────────────────────────────────────────┘
 *         │ napi_schedule()
 *         ▼
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  napi_poll()                【RX 路径】                      │
 *   │    1. 找 DONE + CPU owner 的 RX desc                        │
 *   │    2. dma_unmap_single(RX buffer, DMA_FROM_DEVICE)         │
 *   │    3. skb_put(skb, len)                                    │
 *   │    4. eth_type_trans() → 解析 ethertype                    │
 *   │    5. netif_receive_skb(skb) → 送上协议栈                  │
 *   │    6. stage04_refill_rx_slot() → 立刻补 fresh buffer       │
 *   └─────────────────────────────────────────────────────────────┘
 *
 * 【与 stage03 的关系】
 * stage03: NAPI 为什么需要，interrupt 只做 schedule，poll 按 budget drain
 * stage04: pending queue → descriptor ring；skb 凭空出现 → 预投递 RX buffer
 *
 * 【模块参数】
 *   ifname=xxx    net_device 名称（默认 nds4）
 *   ring_size=N   TX/RX ring 深度（默认 64）
 *   napi_weight=N NAPI poll weight（默认 16）
 *   rx_buf_size=N 预分配 RX buffer 大小（默认 2048）
 *
 * 【关键调试路径】
 *   dmesg | grep stage04  → TX ETH= 和 POLL PROTO= 对比
 *   cat /sys/kernel/debug/netdev_stage04/stats  → 所有计数
 *   cat /sys/kernel/debug/netdev_stage04/rings   → ring 状态 dump
 */
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/u64_stats_sync.h>
#include <linux/version.h>

#define DRV_NAME "netdev_stage04"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define STAGE04_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
	netif_napi_add_weight((ndev), (napi), (pollfn), (weight))
#else
#define STAGE04_NETIF_NAPI_ADD(ndev, napi, pollfn, weight) \
	netif_napi_add((ndev), (napi), (pollfn), (weight))
#endif
#define STAGE04_MAX_RING_DUMP 16

enum stage04_desc_state {
	STAGE04_DESC_EMPTY = 0,
	STAGE04_DESC_POSTED,
	STAGE04_DESC_DONE,
	STAGE04_DESC_BUSY,
};

enum stage04_desc_owner {
	STAGE04_OWNER_CPU = 0,
	STAGE04_OWNER_DEV,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
/* 6.8+: returns flags via return value */
#define STAGE04_U64_UPDATE_BEGIN(syncp, flags) \
	do { (flags) = u64_stats_update_begin_irqsave((syncp)); } while (0)
#define STAGE04_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin((syncp)); } while (0)
#define STAGE04_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry((syncp), (start))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
/* 5.15-6.7: 1-arg form, returns void */
#define STAGE04_U64_UPDATE_BEGIN(syncp, flags) \
	u64_stats_update_begin_irqsave((syncp))
#define STAGE04_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin((syncp)); } while (0)
#define STAGE04_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry((syncp), (start))
#else
/* 5.14 and older: 2-arg form */
#define STAGE04_U64_UPDATE_BEGIN(syncp, flags) \
	u64_stats_update_begin_irqsave((syncp), (flags))
#define STAGE04_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin_irq((syncp)); } while (0)
#define STAGE04_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry_irq((syncp), (start))
#endif

static char ifname[IFNAMSIZ] = "nds4";
module_param_string(ifname, ifname, sizeof(ifname), 0644);
MODULE_PARM_DESC(ifname, "interface name for stage04 ring/dma net_device");

static int ring_size = 64;
module_param(ring_size, int, 0644);
MODULE_PARM_DESC(ring_size, "ring size for TX/RX descriptor arrays");

static int napi_weight = 16;
module_param(napi_weight, int, 0644);
MODULE_PARM_DESC(napi_weight, "NAPI poll weight");

static int rx_buf_size = 2048;
module_param(rx_buf_size, int, 0644);
MODULE_PARM_DESC(rx_buf_size, "RX buffer size for pre-posted descriptors");

/*
 * 【TX descriptor — 记录一次 TX 请求】
 *
 * TX ring 是给 ndo_start_xmit() 用的。TX 路径中：
 *   - CPU 分配 skb，建立 DMA 映射
 *   - 填 TX desc，标记 BUSY + DEV owner
 *   - device（教学型）处理完后，CPU 清理 desc（标记 EMPTY + CPU owner）
 *
 * 在 stage04 中，TX desc 主要用于"记录 DMA 地址"，实际数据 memcpy
 * 是直接写到 RX buffer 的，所以 TX ring 更像是一个"挂起的 xmit 请求"队列。
 */
struct stage04_tx_desc {
	dma_addr_t dma_addr;   /* skb->data 的 DMA 地址，unmap 时用 */
	u32 data_len;         /* 数据长度 */
	u8 owner;             /* OWNER_CPU=0 / OWNER_DEV=1 */
	u8 state;             /* EMPTY(0) / BUSY(2) */
};

/*
 * 【RX descriptor — 预投递的 DMA buffer】
 *
 * RX ring 是 stage04 最重要的数据结构，承载 RX replenishment 循环：
 *
 *   EMPTY ──(refill)──> POSTED + DEV owner
 *      ▲                      │
 *      │                      ▼ (device 写完，通知 CPU)
 *      │                DONE + CPU owner
 *      │                      │
 *      └──────(poll drain)────┘
 *
 * 教学型"device"在 TX 路径中把数据 memcpy 到 posted RX buffer，
 * 然后把 desc 标记为 DONE + CPU owner，poll 线程 drain 这个 desc，
 * 处理完后立刻 refill（重新 post + DEV owner），形成完整的 replenishment 循环。
 */
struct stage04_rx_desc {
	struct sk_buff *skb;   /* 预分配的 skb，data 区域是 DMA buffer */
	dma_addr_t dma_addr;  /* skb->data 的 DMA 地址（device 可写） */
	u32 buf_len;          /* buffer 总大小（rx_buf_size） */
	u32 data_len;         /* 实际数据长度（device 写入了多少） */
	u8 owner;             /* OWNER_CPU=0 / OWNER_DEV=1 */
	u8 state;             /* EMPTY(0) / POSTED(1) / DONE(2) */
};

/*
 * 【priv — 驱动私有数据，每个 netdev 拥有一个】
 *
 * 核心字段分组：
 *   - 设备基础：ndev, dbg_dir, ring_lock, irq_masked
 *   - TX 状态：tx_ring, tx_prod（生产端指针）
 *   - RX 状态：rx_ring, rx_hw_pos（device 写位置）, rx_poll_pos（poll 读位置）
 *              rx_posted（device 所有且未处理的）, rx_done（已处理完待 drain 的）
 *   - 统计计数：tx_*, rx_*, napi_*, irq_* 四组计数器（64bit，支持并发读写）
 *
 * 【TX/RX 指针追踪】
 *   tx_prod：TX ring 生产端，每次 xmit 后 +1（mod ring_size）
 *   rx_hw_pos：RX ring 的"device 写位置"，每次 TX memcpy 后更新为 rx_idx+1
 *   rx_poll_pos：RX poll 读位置，每次 poll 处理一个 desc 后 +1
 *
 * 【rx_posted vs rx_done 的区别】
 *   rx_posted：属于 DEV owner 且未处理（device 已"接收"但 CPU 还没 poll 到）
 *   rx_done：属于 CPU owner 且已标记 DONE（poll 已看到，等待处理）
 *
 *   TX memcpy 完成后：rx_posted--, rx_done++
 *   poll 处理完成后：rx_done--（同时 refill 补回 rx_posted++）
 */
struct stage04_priv {
	struct net_device *ndev;
	struct dentry *dbg_dir;         /* debugfs 根目录 */
	struct napi_struct napi;        /* NAPI poll 结构 */
	struct packet_type rx_pkt_type; /* ethertype 0x88B7 的 packet_type handler */
	spinlock_t ring_lock;           /* 保护 ring 状态的锁 */
	struct u64_stats_sync syncp;    /* 64bit 计数器并发保护 */
	bool irq_masked;                /* NAPI irq 屏蔽标志（避免重复 schedule） */

	/* === TX 状态 === */
	struct stage04_tx_desc *tx_ring;
	u16 ring_size;
	u16 tx_prod;                    /* TX 生产端指针（下次 xmit 写入位置） */

	/* === RX 状态 === */
	struct stage04_rx_desc *rx_ring;
	u16 rx_hw_pos;                  /* RX "device 写位置"（TX 路径更新） */
	u16 rx_poll_pos;                /* RX poll 读位置（poll 路径更新） */
	u16 rx_posted;                   /* 当前 POSTED+DEV owner 的 desc 数量 */
	u16 rx_done;                    /* 当前 DONE+CPU owner 的 desc 数量（待 drain） */

	u32 rx_buf_size;                /* 预分配 RX buffer 大小 */

	u64 tx_packets;
	u64 tx_bytes;
	u64 tx_dropped;
	u64 tx_dma_map_ok;
	u64 tx_dma_map_fail;
	u64 tx_dma_unmap;
	u64 tx_busy_drop;
	u64 tx_linearize_count;
	u64 tx_completion_count;
	u64 last_tx_len;
	u64 last_tx_proto;

	u64 rx_packets;
	u64 rx_bytes;
	u64 rx_dropped;
	u64 rx_dma_map_ok;
	u64 rx_dma_map_fail;
	u64 rx_dma_unmap;
	u64 rx_ring_posted;
	u64 rx_ring_done;
	u64 rx_ring_polled;
	u64 rx_refill_attempts;
	u64 rx_refill_ok;
	u64 rx_refill_fail;
	u64 rx_no_desc_drop;
	u64 rx_truncated;
	u64 last_rx_len;
	u64 last_rx_proto;
	u64 rx_pending_peak;

	u64 irq_raised;
	u64 irq_masked_count;
	u64 irq_unmasked_count;
	u64 napi_schedule_count;
	u64 napi_poll_count;
	u64 napi_complete_count;
	u64 napi_budget_exhaust_count;
	u64 napi_work_total;
	u64 last_poll_budget;
	u64 last_poll_work;

	u64 open_count;
	u64 stop_count;
};

static struct net_device *stage04_dev;

static const char *stage04_desc_state_name(u8 state)
{
	switch (state) {
	case STAGE04_DESC_EMPTY: return "EMPTY";
	case STAGE04_DESC_POSTED: return "POSTED";
	case STAGE04_DESC_DONE: return "DONE";
	case STAGE04_DESC_BUSY: return "BUSY";
	default: return "?";
	}
}

static const char *stage04_desc_owner_name(u8 owner)
{
	switch (owner) {
	case STAGE04_OWNER_CPU: return "CPU";
	case STAGE04_OWNER_DEV: return "DEV";
	default: return "?";
	}
}

static void stage04_count_open(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->open_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_stop(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->stop_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_irq_raised(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->irq_raised++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_irq_masked(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->irq_masked_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_irq_unmasked(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->irq_unmasked_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_napi_schedule(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->napi_schedule_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_napi_poll(struct stage04_priv *priv, int budget,
				    int work_done, bool exhausted)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->napi_poll_count++;
	priv->napi_work_total += work_done;
	priv->last_poll_budget = budget;
	priv->last_poll_work = work_done;
	if (exhausted)
		priv->napi_budget_exhaust_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_napi_complete(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->napi_complete_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_tx_success(struct stage04_priv *priv, unsigned int len, __be16 proto)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->tx_packets++;
	priv->tx_bytes += len;
	priv->last_tx_len = len;
	priv->last_tx_proto = ntohs(proto);
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_tx_drop(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->tx_dropped++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_tx_busy_drop(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->tx_busy_drop++;
	priv->tx_dropped++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_tx_dma_map(struct stage04_priv *priv, bool ok)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	if (ok)
		priv->tx_dma_map_ok++;
	else
		priv->tx_dma_map_fail++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_tx_dma_unmap(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->tx_dma_unmap++;
	priv->tx_completion_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_tx_linearize(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->tx_linearize_count++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_rx_dma_map(struct stage04_priv *priv, bool ok)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	if (ok)
		priv->rx_dma_map_ok++;
	else
		priv->rx_dma_map_fail++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_rx_dma_unmap(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->rx_dma_unmap++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_rx_refill(struct stage04_priv *priv, bool ok)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->rx_refill_attempts++;
	if (ok)
		priv->rx_refill_ok++;
	else
		priv->rx_refill_fail++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_rx_done(struct stage04_priv *priv, unsigned int depth)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->rx_ring_done++;
	if (depth > priv->rx_pending_peak)
		priv->rx_pending_peak = depth;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_rx_posted(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->rx_ring_posted++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_rx_no_desc(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->rx_no_desc_drop++;
	priv->rx_dropped++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_rx_truncated(struct stage04_priv *priv)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->rx_truncated++;
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

static void stage04_count_rx_receive(struct stage04_priv *priv, unsigned int len,
				     __be16 proto, int rc)
{
	unsigned long flags;

	STAGE04_U64_UPDATE_BEGIN(&priv->syncp, flags);
	priv->rx_ring_polled++;
	if (rc == NET_RX_SUCCESS) {
		priv->rx_packets++;
		priv->rx_bytes += len;
		priv->last_rx_len = len;
		priv->last_rx_proto = ntohs(proto);
	} else {
		priv->rx_dropped++;
	}
	u64_stats_update_end_irqrestore(&priv->syncp, flags);
}

/*
 * 【DMA capability 准备】
 *
 * net_device 本身没有 PCI 父设备，所以要手动设置 DMA mask：
 *   1. 先尝试 64bit mask（现代系统）
 *   2. 失败则 fallback 到 32bit（老系统或虚拟化环境）
 *
 * 注意：这里是"尽力而为"，smoke 时才会暴露真实支持情况。
 */
static int stage04_prepare_dma_caps(struct net_device *ndev)
{
	int ret;

	ndev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	ndev->dev.dma_mask = &ndev->dev.coherent_dma_mask;
	ret = dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(64));
	if (ret)
		ret = dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(32));
	return ret;
}

/*
 * 【RX slot replenishment — 重新补 fresh buffer 回 slot】
 *
 * 这是 RX replenishment 循环的核心。poll 处理完一个 RX desc 后，
 * 必须立刻调用本函数把同一个 slot 重新填满，保证 ring 始终满。
 *
 * 流程：
 *   1. 分配新 skb（netdev_alloc_skb_ip_align 自动对齐）
 *   2. dma_map_single(skb->data, DMA_FROM_DEVICE) — 建立 DMA 映射，device 可写
 *   3. dma_sync_single_for_device — 确保 device 看到 DMA memory
 *   4. 标记 POSTED + DEV owner（交给 device）
 *
 * 【关键点】
 *   - DMA_FROM_DEVICE 表示这个 buffer 是给 device 写的
 *   - owner=DEV 保证 TX 路径 memcpy 时不会误用
 *   - refill 失败（ENOMEM）会导致 rx_no_desc_drop 递增
 */
static int stage04_refill_rx_slot(struct stage04_priv *priv, u16 idx)
{
	struct stage04_rx_desc *rxd = &priv->rx_ring[idx];
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	unsigned long irq_flags;

	/* 分配 skb，data 区域将作为 DMA buffer */
	skb = netdev_alloc_skb_ip_align(priv->ndev, priv->rx_buf_size);
	if (!skb) {
		stage04_count_rx_refill(priv, false);
		return -ENOMEM;
	}

	/* 建立 DMA 映射，DMA_FROM_DEVICE = device 可写 */
	dma_addr = dma_map_single(&priv->ndev->dev, skb->data, priv->rx_buf_size,
				  DMA_FROM_DEVICE);
	if (dma_mapping_error(&priv->ndev->dev, dma_addr)) {
		dev_kfree_skb_any(skb);
		stage04_count_rx_dma_map(priv, false);
		stage04_count_rx_refill(priv, false);
		return -EIO;
	}
	stage04_count_rx_dma_map(priv, true);
	stage04_count_rx_refill(priv, true);

	/* 同步 DMA buffer，让 device 看到最新数据 */
	dma_sync_single_for_device(&priv->ndev->dev, dma_addr, priv->rx_buf_size,
				   DMA_FROM_DEVICE);

	/* 填入 desc，标记 POSTED + DEV owner（device 可以写这块 buffer） */
	spin_lock_irqsave(&priv->ring_lock, irq_flags);
	rxd->skb = skb;
	rxd->dma_addr = dma_addr;
	rxd->buf_len = priv->rx_buf_size;
	rxd->data_len = 0;
	rxd->owner = STAGE04_OWNER_DEV;
	rxd->state = STAGE04_DESC_POSTED;
	priv->rx_posted++;
	spin_unlock_irqrestore(&priv->ring_lock, irq_flags);

	stage04_count_rx_posted(priv);
	return 0;
}

/*
 * 【查找 POSTED + DEV owner 的 RX slot】
 *
 * TX 路径在 memcpy 之前必须找到一个"device 可以写"的 RX slot。
 * 从 rx_hw_pos 开始轮询（最多 ring_size 次），找同时满足：
 *   - state == POSTED
 *   - owner == DEV
 *   - skb != NULL
 *
 * 【rx_hw_pos 的作用】
 *   rx_hw_pos 是 device 最近写入的位置（tx 路径更新）。
 *   从这个位置开始查找可以利用空间局部性（新 slot > 旧 slot）。
 *
 * 【返回】
 *   0 = 成功（*slot 填入 idx）
 *   -ENOSPC = 没有可用的 posted slot（RX buffer 耗尽）
 */
static int stage04_find_posted_rx_slot(struct stage04_priv *priv, u16 *slot)
{
	u16 i;

	for (i = 0; i < priv->ring_size; ++i) {
		u16 idx = (priv->rx_hw_pos + i) % priv->ring_size;
		struct stage04_rx_desc *rxd = &priv->rx_ring[idx];

		if (rxd->state == STAGE04_DESC_POSTED &&
		    rxd->owner == STAGE04_OWNER_DEV && rxd->skb) {
			*slot = idx;
			return 0;
		}
	}
	return -ENOSPC;
}

/*
 * 【packet_type handler — 让 ethertype 0x88B7 不被当作"未知协议"丢弃】
 *
 * netif_receive_skb() 收到 skb 后，根据 skb->protocol 查找注册的 packet_type.func。
 * 如果 ethertype 没有注册，skb 被当作"未知协议"直接 drop。
 *
 * 0x88B7 是实验私有协议，内核没有内置 handler，必须自己注册。
 * 但注意：skb 已经在 poll 中通过 netif_receive_skb() 送上了协议栈，
 * 这里注册的 handler 只是让 0x88B7 在 netif_receive_skb 内部查找时能找到。
 *
 * 返回 NET_RX_SUCCESS = "已处理，不要 drop"（实际是个 noop）
 */
static int stage04_rx_pkt_type_func(struct sk_buff *skb, struct net_device *dev,
				     struct packet_type *ptype,
				     struct net_device *orig_dev)
{
	return NET_RX_SUCCESS;
}

/*
 * 【模拟设备中断 — 触发 NAPI poll】
 *
 * 教学型"设备"在 TX memcpy 完成后调用这个函数，模拟硬件 raise irq。
 *
 * 【irq_masked 的作用】
 *   - 防止在 NAPI poll 期间重复 schedule（poll 期间 irq_masked=true）
 *   - poll complete 后 irq_masked=false，重新允许 schedule
 *   - 类似于硬件 NIC 的 interrupt coalescing
 */
static void stage04_raise_irq(struct stage04_priv *priv)
{
	unsigned long flags;
	bool do_schedule = false;

	/* irq_masked 防止 poll 期间重复 schedule */
	spin_lock_irqsave(&priv->ring_lock, flags);
	if (!priv->irq_masked) {
		priv->irq_masked = true;
		do_schedule = true;
	}
	spin_unlock_irqrestore(&priv->ring_lock, flags);

	if (!do_schedule)
		return;

	stage04_count_irq_raised(priv);
	stage04_count_irq_masked(priv);
	stage04_count_napi_schedule(priv);
	napi_schedule(&priv->napi);
}

/*
 * 【NAPI poll — 按 budget 批量 drain RX done descriptors】
 *
 * 这是连接 stage03（NAPI）和 stage04（ring + replenishment）的核心函数。
 *
 * 循环逻辑（每处理一个 desc）：
 *   1. 检查 DONE + CPU owner（不满足则 break）
 *   2. 取出 skb，清空 desc
 *   3. dma_unmap_single（解除 DMA 映射）
 *   4. skb_put（设置实际数据长度）
 *   5. eth_type_trans（剥除 Ethernet header，设置 skb->protocol）
 *   6. netif_receive_skb（送上协议栈）
 *   7. stage04_refill_rx_slot（立刻补 fresh buffer ← RX replenishment 核心）
 *
 * 【budget 耗尽时】
 *   work_done == budget → exhausted=true → 不调用 napi_complete_done
 *   下次 irq 或 softirq 继续 poll
 *
 * 【rx_done 的管理】
 *   TX memcpy 完成后：rx_done++
 *   poll 处理完一个 desc 后：rx_done--
 *   rx_done 反映"已 DONE 待 drain"的 desc 数量
 */
static int stage04_poll(struct napi_struct *napi, int budget)
{
	struct stage04_priv *priv = container_of(napi, struct stage04_priv, napi);
	int work_done = 0;
	bool exhausted = false;

	while (work_done < budget) {
		struct stage04_rx_desc *rxd;
		struct sk_buff *skb;
		dma_addr_t dma_addr;
		u16 idx;
		u32 len;
		u32 buf_len;
		__be16 proto;
		unsigned long flags;
		int rc;

		/* 1. 取当前 poll 位置的 desc，必须是 DONE + CPU owner */
		spin_lock_irqsave(&priv->ring_lock, flags);
		idx = priv->rx_poll_pos;
		rxd = &priv->rx_ring[idx];
		if (rxd->state != STAGE04_DESC_DONE ||
		    rxd->owner != STAGE04_OWNER_CPU || !rxd->skb) {
			spin_unlock_irqrestore(&priv->ring_lock, flags);
			break;  /* 没有更多 done desc，退出循环 */
		}

		/* 2. 取出信息，清空 desc 避免重复处理 */
		skb = rxd->skb;
		dma_addr = rxd->dma_addr;
		len = rxd->data_len;
		buf_len = rxd->buf_len;
		priv->rx_poll_pos = (priv->rx_poll_pos + 1) % priv->ring_size;
		if (priv->rx_done > 0)
			priv->rx_done--;
		rxd->skb = NULL;
		rxd->dma_addr = 0;
		rxd->buf_len = 0;
		rxd->data_len = 0;
		rxd->owner = STAGE04_OWNER_CPU;
		rxd->state = STAGE04_DESC_EMPTY;
		spin_unlock_irqrestore(&priv->ring_lock, flags);

		/* 3. DMA 解映射（device → CPU 方向完成） */
		dma_unmap_single(&priv->ndev->dev, dma_addr, buf_len, DMA_FROM_DEVICE);
		stage04_count_rx_dma_unmap(priv);

		/* 4. 设置 skb 实际数据长度 */
		skb_put(skb, len);

		/* 5. eth_type_trans：剥除 Ethernet header，设置 skb->protocol */
		proto = eth_type_trans(skb, priv->ndev);
		pr_info("[stage04] POLL IDX=%u LEN=%u PROTO=%04x RC=%d\n",
			idx, len, ntohs(proto), rc);

		/* 6. 送上 Linux 协议栈 */
		rc = netif_receive_skb(skb);
		stage04_count_rx_receive(priv, len, proto, rc);

		/* 7. 立刻 refill（RX replenishment 核心） */
		if (stage04_refill_rx_slot(priv, idx))
			netdev_warn(priv->ndev, "refill rx slot %u failed\n", idx);
		work_done++;
	}

	if (work_done == budget)
		exhausted = true;
	stage04_count_napi_poll(priv, budget, work_done, exhausted);

	/* budget 未耗尽且没有更多 done desc → 关闭 NAPI */
	if (!exhausted) {
		unsigned long flags;
		bool has_more = false;
		struct stage04_rx_desc *rxd;

		spin_lock_irqsave(&priv->ring_lock, flags);
		rxd = &priv->rx_ring[priv->rx_poll_pos];
		if (rxd->state == STAGE04_DESC_DONE && rxd->owner == STAGE04_OWNER_CPU)
			has_more = true;
		if (!has_more) {
			napi_complete_done(napi, work_done);
			priv->irq_masked = false;  /* 重新允许 raise_irq */
		}
		spin_unlock_irqrestore(&priv->ring_lock, flags);

		if (!has_more) {
			stage04_count_napi_complete(priv);
			stage04_count_irq_unmasked(priv);
		}
	}

	return work_done;
}

/*
 * 【TX 路径 — ndo_start_xmit 实现】
 *
 * 整体流程（TX 复用 RX ring 作为"device 内存"）：
 *   1. skb_linearize（非线性 skb 处理）
 *   2. dma_map_single(skb->data, DMA_TO_DEVICE)
 *   3. 找 TX ring 槽位（检查 BUSY）
 *   4. 找 POSTED+DEV owner 的 RX slot（TX memcpy 目标）
 *   5. memcpy(skb->data → RX buffer）← 模拟 device DMA
 *   6. RX desc: DONE + CPU owner，rx_posted--, rx_done++
 *   7. dma_unmap_single(TX)
 *   8. stage04_raise_irq() → napi_schedule()
 *
 * 【TX 为什么复用 RX ring？】
 *   stage04 没有真实硬件，"device"需要一个地方来存"接收"的数据。
 *   RX ring 预分配的 posted buffer 天然适合作为这个"device 内存"。
 *   这样一套 ring 两用：TX 用 RX buffer 做 copy destination。
 *
 * 【调试关键】
 *   ETH=%04x 打印的是 skb->data[ETH_HLEN..ETH_HLEN+1]（ethertype 字段）
 *   如果 send_stage04_frame 发送正确，这里应该显示 88b7
 */
static netdev_tx_t stage04_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct stage04_priv *priv = netdev_priv(ndev);
	struct stage04_tx_desc *txd;
	struct stage04_rx_desc *rxd;
	dma_addr_t tx_dma;
	u16 tx_idx;
	u16 rx_idx;
	u32 copy_len;
	unsigned long flags;

	/* 1. 非线性 skb 处理（TX DMA 需要线性地址） */
	if (unlikely(skb_is_nonlinear(skb))) {
		if (skb_linearize(skb)) {
			stage04_count_tx_drop(priv);
			dev_kfree_skb_any(skb);
			return NETDEV_TX_OK;
		}
		stage04_count_tx_linearize(priv);
	}

	stage04_count_tx_success(priv, skb->len, skb->protocol);

	/* 2. DMA 映射 TX skb（CPU → device 方向） */
	tx_dma = dma_map_single(&ndev->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(&ndev->dev, tx_dma)) {
		stage04_count_tx_dma_map(priv, false);
		stage04_count_tx_drop(priv);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	stage04_count_tx_dma_map(priv, true);
	dma_sync_single_for_device(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);

	/* 3. 取 TX desc，检查 ring 是否满 */
	spin_lock_irqsave(&priv->ring_lock, flags);
	tx_idx = priv->tx_prod;
	txd = &priv->tx_ring[tx_idx];
	if (txd->state == STAGE04_DESC_BUSY) {
		spin_unlock_irqrestore(&priv->ring_lock, flags);
		dma_unmap_single(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);
		stage04_count_tx_dma_unmap(priv);
		stage04_count_tx_busy_drop(priv);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* 填 TX desc（标记 BUSY + DEV owner） */
	txd->dma_addr = tx_dma;
	txd->data_len = skb->len;
	txd->owner = STAGE04_OWNER_DEV;
	txd->state = STAGE04_DESC_BUSY;

	/* 4. 找 POSTED+DEV owner 的 RX slot（TX memcpy 目标） */
	if (stage04_find_posted_rx_slot(priv, &rx_idx)) {
		/* 没有可用 RX slot → 丢包（rx_no_desc_drop 计数） */
		txd->owner = STAGE04_OWNER_CPU;
		txd->state = STAGE04_DESC_EMPTY;
		spin_unlock_irqrestore(&priv->ring_lock, flags);
		dma_unmap_single(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);
		stage04_count_tx_dma_unmap(priv);
		stage04_count_rx_no_desc(priv);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* 5. memcpy → RX buffer（模拟 device DMA copy） */
	rxd = &priv->rx_ring[rx_idx];
	copy_len = min_t(u32, skb->len, rxd->buf_len);
	if (copy_len < skb->len)
		stage04_count_rx_truncated(priv);

	dma_sync_single_for_device(&ndev->dev, rxd->dma_addr, rxd->buf_len,
				   DMA_FROM_DEVICE);
	memcpy(rxd->skb->data, skb->data, copy_len);
	dma_sync_single_for_cpu(&ndev->dev, rxd->dma_addr, copy_len,
			    DMA_FROM_DEVICE);

	/* 6. 标记 RX desc DONE + CPU owner，触发 NAPI */
	rxd->data_len = copy_len;
	pr_info("[stage04] TX RXIDX=%u SKBLEN=%u CPYLEN=%u ETH=%04x\n",
		rx_idx, skb->len, copy_len,
		ntohs(*(__be16 *)(skb->data + ETH_HLEN)));
	rxd->owner = STAGE04_OWNER_CPU;
	rxd->state = STAGE04_DESC_DONE;
	if (priv->rx_posted > 0)
		priv->rx_posted--;
	priv->rx_done++;
	priv->rx_hw_pos = (rx_idx + 1) % priv->ring_size;
	stage04_count_rx_done(priv, priv->rx_done);

	/* 7. 释放 TX DMA 映射，清理 TX desc */
	dma_unmap_single(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);
	txd->dma_addr = 0;
	txd->data_len = 0;
	txd->owner = STAGE04_OWNER_CPU;
	txd->state = STAGE04_DESC_EMPTY;
	priv->tx_prod = (priv->tx_prod + 1) % priv->ring_size;
	spin_unlock_irqrestore(&priv->ring_lock, flags);

	stage04_count_tx_dma_unmap(priv);
	dev_consume_skb_any(skb);
	stage04_raise_irq(priv);
	return NETDEV_TX_OK;
}

static int stage04_open(struct net_device *ndev)
{
	struct stage04_priv *priv = netdev_priv(ndev);

	netif_start_queue(ndev);
	napi_enable(&priv->napi);
	stage04_count_open(priv);
	return 0;
}

static int stage04_stop(struct net_device *ndev)
{
	struct stage04_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	napi_disable(&priv->napi);
	stage04_count_stop(priv);
	return 0;
}

static void stage04_get_stats64(struct net_device *ndev,
				struct rtnl_link_stats64 *stats)
{
	struct stage04_priv *priv = netdev_priv(ndev);
	unsigned int start;

	do {
		STAGE04_U64_FETCH_BEGIN(&priv->syncp, start);
		stats->tx_packets = priv->tx_packets;
		stats->tx_bytes = priv->tx_bytes;
		stats->tx_dropped = priv->tx_dropped;
		stats->rx_packets = priv->rx_packets;
		stats->rx_bytes = priv->rx_bytes;
		stats->rx_dropped = priv->rx_dropped;
	} while (STAGE04_U64_FETCH_RETRY(&priv->syncp, start));
}

static const struct net_device_ops stage04_netdev_ops = {
	.ndo_open		= stage04_open,
	.ndo_stop		= stage04_stop,
	.ndo_start_xmit		= stage04_start_xmit,
	.ndo_get_stats64	= stage04_get_stats64,
};

static int stage04_stats_show(struct seq_file *m, void *v)
{
	struct stage04_priv *priv = m->private;
	unsigned int start;

	do {
		STAGE04_U64_FETCH_BEGIN(&priv->syncp, start);
		seq_printf(m, "ifname=%s\n", priv->ndev->name);
		seq_printf(m, "ring_size=%u\n", priv->ring_size);
		seq_printf(m, "napi_weight=%d\n", napi_weight);
		seq_printf(m, "rx_buf_size=%u\n", priv->rx_buf_size);
		seq_printf(m, "tx_packets=%llu\n", priv->tx_packets);
		seq_printf(m, "tx_bytes=%llu\n", priv->tx_bytes);
		seq_printf(m, "tx_dropped=%llu\n", priv->tx_dropped);
		seq_printf(m, "tx_busy_drop=%llu\n", priv->tx_busy_drop);
		seq_printf(m, "tx_dma_map_ok=%llu\n", priv->tx_dma_map_ok);
		seq_printf(m, "tx_dma_map_fail=%llu\n", priv->tx_dma_map_fail);
		seq_printf(m, "tx_dma_unmap=%llu\n", priv->tx_dma_unmap);
		seq_printf(m, "tx_linearize_count=%llu\n", priv->tx_linearize_count);
		seq_printf(m, "tx_completion_count=%llu\n", priv->tx_completion_count);
		seq_printf(m, "last_tx_len=%llu\n", priv->last_tx_len);
		seq_printf(m, "last_tx_proto=%llu\n", priv->last_tx_proto);
		seq_printf(m, "rx_packets=%llu\n", priv->rx_packets);
		seq_printf(m, "rx_bytes=%llu\n", priv->rx_bytes);
		seq_printf(m, "rx_dropped=%llu\n", priv->rx_dropped);
		seq_printf(m, "rx_dma_map_ok=%llu\n", priv->rx_dma_map_ok);
		seq_printf(m, "rx_dma_map_fail=%llu\n", priv->rx_dma_map_fail);
		seq_printf(m, "rx_dma_unmap=%llu\n", priv->rx_dma_unmap);
		seq_printf(m, "rx_ring_posted=%llu\n", priv->rx_ring_posted);
		seq_printf(m, "rx_ring_done=%llu\n", priv->rx_ring_done);
		seq_printf(m, "rx_ring_polled=%llu\n", priv->rx_ring_polled);
		seq_printf(m, "rx_refill_attempts=%llu\n", priv->rx_refill_attempts);
		seq_printf(m, "rx_refill_ok=%llu\n", priv->rx_refill_ok);
		seq_printf(m, "rx_refill_fail=%llu\n", priv->rx_refill_fail);
		seq_printf(m, "rx_no_desc_drop=%llu\n", priv->rx_no_desc_drop);
		seq_printf(m, "rx_truncated=%llu\n", priv->rx_truncated);
		seq_printf(m, "last_rx_len=%llu\n", priv->last_rx_len);
		seq_printf(m, "last_rx_proto=%llu\n", priv->last_rx_proto);
		seq_printf(m, "rx_pending_peak=%llu\n", priv->rx_pending_peak);
		seq_printf(m, "irq_raised=%llu\n", priv->irq_raised);
		seq_printf(m, "irq_masked_count=%llu\n", priv->irq_masked_count);
		seq_printf(m, "irq_unmasked_count=%llu\n", priv->irq_unmasked_count);
		seq_printf(m, "napi_schedule_count=%llu\n", priv->napi_schedule_count);
		seq_printf(m, "napi_poll_count=%llu\n", priv->napi_poll_count);
		seq_printf(m, "napi_complete_count=%llu\n", priv->napi_complete_count);
		seq_printf(m, "napi_budget_exhaust_count=%llu\n", priv->napi_budget_exhaust_count);
		seq_printf(m, "napi_work_total=%llu\n", priv->napi_work_total);
		seq_printf(m, "last_poll_budget=%llu\n", priv->last_poll_budget);
		seq_printf(m, "last_poll_work=%llu\n", priv->last_poll_work);
		seq_printf(m, "open_count=%llu\n", priv->open_count);
		seq_printf(m, "stop_count=%llu\n", priv->stop_count);
		seq_printf(m, "rx_posted_current=%u\n", priv->rx_posted);
		seq_printf(m, "rx_done_current=%u\n", priv->rx_done);
		seq_printf(m, "irq_masked=%u\n", priv->irq_masked ? 1 : 0);
	} while (STAGE04_U64_FETCH_RETRY(&priv->syncp, start));

	return 0;
}

static int stage04_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, stage04_stats_show, inode->i_private);
}

static const struct file_operations stage04_stats_fops = {
	.owner = THIS_MODULE,
	.open = stage04_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int stage04_rings_show(struct seq_file *m, void *v)
{
	struct stage04_priv *priv = m->private;
	unsigned long flags;
	u16 i;
	u16 dump_nr;

	spin_lock_irqsave(&priv->ring_lock, flags);
	seq_printf(m, "tx_prod=%u rx_hw_pos=%u rx_poll_pos=%u rx_posted=%u rx_done=%u\n",
		   priv->tx_prod, priv->rx_hw_pos, priv->rx_poll_pos,
		   priv->rx_posted, priv->rx_done);

	dump_nr = min_t(u16, priv->ring_size, STAGE04_MAX_RING_DUMP);
	seq_puts(m, "\n[TX ring]\n");
	for (i = 0; i < dump_nr; ++i) {
		struct stage04_tx_desc *txd = &priv->tx_ring[i];
		seq_printf(m, "tx[%u] owner=%s state=%s len=%u dma=0x%llx\n",
			   i, stage04_desc_owner_name(txd->owner),
			   stage04_desc_state_name(txd->state), txd->data_len,
			   (unsigned long long)txd->dma_addr);
	}

	seq_puts(m, "\n[RX ring]\n");
	for (i = 0; i < dump_nr; ++i) {
		struct stage04_rx_desc *rxd = &priv->rx_ring[i];
		seq_printf(m,
			   "rx[%u] owner=%s state=%s len=%u buf=%u dma=0x%llx skb=%px\n",
			   i, stage04_desc_owner_name(rxd->owner),
			   stage04_desc_state_name(rxd->state), rxd->data_len,
			   rxd->buf_len, (unsigned long long)rxd->dma_addr, rxd->skb);
	}
	spin_unlock_irqrestore(&priv->ring_lock, flags);

	return 0;
}

static int stage04_rings_open(struct inode *inode, struct file *file)
{
	return single_open(file, stage04_rings_show, inode->i_private);
}

static const struct file_operations stage04_rings_fops = {
	.owner = THIS_MODULE,
	.open = stage04_rings_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void stage04_debugfs_init(struct stage04_priv *priv)
{
	priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
	if (IS_ERR_OR_NULL(priv->dbg_dir)) {
		priv->dbg_dir = NULL;
		return;
	}

	debugfs_create_file("stats", 0444, priv->dbg_dir, priv, &stage04_stats_fops);
	debugfs_create_file("rings", 0444, priv->dbg_dir, priv, &stage04_rings_fops);
}

static void stage04_debugfs_exit(struct stage04_priv *priv)
{
	debugfs_remove_recursive(priv->dbg_dir);
	priv->dbg_dir = NULL;
}

static void stage04_cleanup_rings(struct stage04_priv *priv)
{
	u16 i;

	if (priv->rx_ring) {
		for (i = 0; i < priv->ring_size; ++i) {
			struct stage04_rx_desc *rxd = &priv->rx_ring[i];
			if (rxd->dma_addr)
				dma_unmap_single(&priv->ndev->dev, rxd->dma_addr,
						 rxd->buf_len ?: priv->rx_buf_size,
						 DMA_FROM_DEVICE);
			if (rxd->skb)
				dev_kfree_skb_any(rxd->skb);
			rxd->skb = NULL;
			rxd->dma_addr = 0;
		}
		kfree(priv->rx_ring);
		priv->rx_ring = NULL;
	}

	if (priv->tx_ring) {
		for (i = 0; i < priv->ring_size; ++i) {
			struct stage04_tx_desc *txd = &priv->tx_ring[i];
			if (txd->dma_addr)
				dma_unmap_single(&priv->ndev->dev, txd->dma_addr,
						 txd->data_len, DMA_TO_DEVICE);
		}
		kfree(priv->tx_ring);
		priv->tx_ring = NULL;
	}
}

/*
 * 【ring 初始化 — 分配 TX/RX ring 并预投递所有 RX buffer】
 *
 * 流程：
 *   1. kcalloc 分配 TX/RX ring（GFP_KERNEL）
 *   2. 对每个 RX slot 调用 stage04_refill_rx_slot()（预投递）
 *
 * 关键点：RX ring 在 init 时就全部填满（POSTED + DEV owner）。
 * 这是 RX replenishment 循环的前提——device 有 buffer 可写。
 */
static int stage04_init_rings(struct stage04_priv *priv)
{
	u16 i;
	int ret;

	priv->tx_ring = kcalloc(priv->ring_size, sizeof(*priv->tx_ring), GFP_KERNEL);
	priv->rx_ring = kcalloc(priv->ring_size, sizeof(*priv->rx_ring), GFP_KERNEL);
	if (!priv->tx_ring || !priv->rx_ring)
		return -ENOMEM;

	for (i = 0; i < priv->ring_size; ++i) {
		ret = stage04_refill_rx_slot(priv, i);
		if (ret)
			return ret;
	}
	return 0;
}

/*
 * 【模块初始化 — 分配 netdev → 设置属性 → 初始化 ring → 注册】
 *
 * init 顺序（严格对应 exit 的逆序）：
 *   1. alloc_etherdev_mqs — 分配含 priv 的 netdev
 *   2. DMA capability — 设置 DMA mask
 *   3. netdev_ops — 注册 ndo_start_xmit / ndo_open / ndo_stop
 *   4. spin_lock_init / u64_stats_init — 初始化并发保护
 *   5. STAGE04_NETIF_NAPI_ADD — 注册 NAPI poll 回调
 *   6. stage04_init_rings — 分配 TX/RX ring + 预投递 RX buffer
 *   7. register_netdev — 把 netdev 注入内核
 *   8. dev_add_pack — 注册 ethertype 0x88B7 的 packet_type handler
 *   9. debugfs_create_dir — 创建 debugfs 节点
 *
 * 注意：register_netdev 之后才能调用 dev_add_pack，
 * 因为 packet_type.func 中可能访问到未注册的 netdev。
 */
static int __init stage04_init(void)
{
	struct net_device *ndev;
	struct stage04_priv *priv;
	int ret;

	/* 参数合法性检查 */
	if (ring_size < 4)
		ring_size = 4;
	if (ring_size > 1024)
		ring_size = 1024;
	if (rx_buf_size < 256)
		rx_buf_size = 256;
	if (napi_weight < 1)
		napi_weight = 1;

	/* 1. 分配 netdev（含 stage04_priv）*/
	ndev = alloc_etherdev_mqs(sizeof(*priv), 1, 1);
	if (!ndev)
		return -ENOMEM;

	/* 2. 设置 netdev 属性 */
	snprintf(ndev->name, IFNAMSIZ, "%s", ifname);
	eth_hw_addr_random(ndev);
	ndev->netdev_ops = &stage04_netdev_ops;
	ndev->mtu = 1500;
	ndev->min_mtu = 68;
	ndev->max_mtu = 1500;
	ndev->flags |= IFF_NOARP;
	ndev->features |= NETIF_F_HW_CSUM;

	/* 3. DMA capability */
	ret = stage04_prepare_dma_caps(ndev);
	if (ret)
		netdev_warn(ndev, "dma_set_mask_and_coherent failed: %d\n", ret);

	/* 4. 初始化 priv */
	priv = netdev_priv(ndev);
	memset(priv, 0, sizeof(*priv));
	priv->ndev = ndev;
	priv->ring_size = ring_size;
	priv->rx_buf_size = rx_buf_size;
	spin_lock_init(&priv->ring_lock);
	u64_stats_init(&priv->syncp);
	STAGE04_NETIF_NAPI_ADD(ndev, &priv->napi, stage04_poll, napi_weight);

	/* 5. 初始化 ring */
	ret = stage04_init_rings(priv);
	if (ret)
		goto err_napi;

	/* 6. 注册 netdev */
	ret = register_netdev(ndev);
	if (ret)
		goto err_rings;

	/* 7. 注册 packet_type handler（ethertype 0x88B7） */
	priv->rx_pkt_type.type = htons(0x88B7);
	priv->rx_pkt_type.dev = ndev;
	priv->rx_pkt_type.func = stage04_rx_pkt_type_func;
	dev_add_pack(&priv->rx_pkt_type);

	/* 8. debugfs */
	stage04_debugfs_init(priv);
	stage04_dev = ndev;
	pr_info("[%s] registered ifname=%s ring_size=%d napi_weight=%d rx_buf_size=%d\n",
		DRV_NAME, ndev->name, ring_size, napi_weight, rx_buf_size);
	return 0;

err_rings:
	stage04_cleanup_rings(priv);
err_napi:
	netif_napi_del(&priv->napi);
	free_netdev(ndev);
	return ret;
}

/*
 * 【模块卸载 — 严格遵循逆序清理】
 *
 * exit 顺序（与 init 严格逆序，错误顺序会导致 rmmod 卡死 used=-1）：
 *
 *   1. napi_disable        — 禁止新的 poll 调用（CPU 无法再 schedule 本 NAPI）
 *   2. netif_tx_disable    — 禁止新的 xmit 调用
 *   3. unregister_netdev   — 注销设备（内核等待所有 dev_hold 释放）
 *   4. dev_remove_pack     — 移除 ethertype handler（新包不再进来）
 *   5. debugfs_remove_recursive — 清理 debugfs
 *   6. netif_napi_del      — 从系统移除 NAPI 结构
 *   7. stage04_cleanup_rings — 释放 ring 内存（unmap DMA，kfree skb）
 *   8. free_netdev         — 释放 netdev
 *
 * 【错误顺序的后果】
 *   - napi_disable 太晚：poll 还在跑时 ring 被释放 → UAF crash
 *   - dev_remove_pack 太早：新包进来找不到 handler → crash
 *   - unregister_netdev 前未 napi_disable：死锁（used=-1）
 */
static void __exit stage04_exit(void)
{
	struct stage04_priv *priv;

	if (!stage04_dev)
		return;

	priv = netdev_priv(stage04_dev);

	/* 1. 停止 NAPI polling（禁止新的 poll 调用） */
	napi_disable(&priv->napi);
	/* 2. 停止 TX queue（禁止新的 xmit 调用） */
	netif_tx_disable(stage04_dev);
	/* 3. 注销 netdev（内核等待所有引用释放后才返回） */
	unregister_netdev(stage04_dev);
	/* 4. 移除 packet_type handler */
	dev_remove_pack(&priv->rx_pkt_type);
	/* 5. 清理 debugfs */
	stage04_debugfs_exit(priv);
	/* 6. 从系统移除 NAPI */
	netif_napi_del(&priv->napi);
	/* 7. 释放 ring（unmap DMA + kfree skb） */
	stage04_cleanup_rings(priv);
	/* 8. 释放 netdev */
	free_netdev(stage04_dev);
	stage04_dev = NULL;
	pr_info("[%s] unloaded\n", DRV_NAME);
}

module_init(stage04_init);
module_exit(stage04_exit);

MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("stage04 ring/dma/replenishment teaching netdev");
MODULE_LICENSE("GPL");
