// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage07.c — stage07 real queue model 教学驱动
 *
 * 【教学目标】
 * 本驱动在 stage04 的 ring + DMA + RX replenishment 基础上，进一步明确：
 *   1. TX 队列生命周期：submit_idx（CPU提交）→ notify_idx（backend消费）→ complete_idx（CPU回收）
 *   2. RX 队列生命周期：post_idx（CPU补充）→ device_idx（backend写入）→ consume_idx（CPU消费）
 *   3. 用 6 个显式 index 把"谁生产、谁消费、谁通知、谁回收"讲清楚
 *   4. NAPI 只做批处理，IRQ 只负责叫醒，两者边界清晰
 *
 * 【整体数据路径】
 *
 *   userspace send_stage07_frame
 *         │
 *         ▼
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  ndo_start_xmit()           【TX 路径·CPU侧】              │
 *   │    1. skb_linearize（处理非线性 skb）                       │
 *   │    2. dma_map_single(skb->data, DMA_TO_DEVICE)              │
 *   │    3. 找一个 FREE 的 TX slot                               │
 *   │    4. slot[dma_addr, len, skb] = SUBMITTED                 │
 *   │    5. submit_idx++（推进 CPU 提交指针）                     │
 *   │    6. tx_inflight++                                       │
 *   │    7. stage07_kick_device() → 通知 backend 处理            │
 *   └─────────────────────────────────────────────────────────────┘
 *         │ 同步 memcpy 模拟 device DMA
 *         ▼
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  stage07_kick_device()     【TX+RX 路径·backend侧】        │
 *   │    TX: slot = DONE（标记已完成）                           │
 *   │    RX: 从 POSTED slot 取 buffer → memcpy(TX data → RX)     │
 *   │    RX: slot = DONE（标记已填数据）                         │
 *   │    TX: tx_done++，notify_idx++（backend 消费进度）          │
 *   │    RX: rx_ready++，device_idx++（backend 写入进度）         │
 *   │    如果有进展 → stage07_raise_irq()                        │
 *   └─────────────────────────────────────────────────────────────┘
 *         │ napi_schedule()
 *         ▼
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  napi_poll()                【TX+RX 路径·CPU侧·NAPI】      │
 *   │    TX: stage07_complete_tx_one() → DMA unmap → free skb    │
 *   │         complete_idx++，tx_inflight--                       │
 *   │    RX: stage07_consume_rx_one() → netif_receive_skb()      │
 *   │         consume_idx++，rx_ready--                           │
 *   │         触发 stage07_refill_one() → post_idx++             │
 *   └─────────────────────────────────────────────────────────────┘
 *
 * 【与 stage04 的关系】
 * stage04: owner 模型（owner=CPU/DEV），TX 复用 RX slot 作为 copy 目标
 * stage07: index 模型（6个显式 index），TX/RX queue 完全独立
 *
 * 【模块参数】
 *   ifname=xxx      net_device 名称（默认 nds7）
 *   ring_size=N     TX/RX queue 深度（默认 128）
 *   napi_weight=N   NAPI poll weight（默认 32）
 *   rx_buf_size=N   预分配 RX buffer 大小（默认 2048）
 *
 * 【关键调试路径】
 *   dmesg | grep stage07         → 模块加载/卸载日志
 *   cat /sys/kernel/debug/netdev_stage07/stats   → 所有计数
 *   cat /sys/kernel/debug/netdev_stage07/queues  → TX/RX queue dump
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
#include <linux/version.h>

#include "../include/netdev_stage07_compat.h"

/*========================== 常量与宏定义 ==========================*/

/* 驱动名称，用于 pr_info / debugfs / module 名 */
#define DRV_NAME "netdev_stage07"

/* TX/RX queue 默认深度（必须是 2 的幂，用于 ring index wrap-around） */
#define STAGE07_DEFAULT_RING_SIZE 128

/* NAPI poll 的 budget（每轮 poll 最多处理的 RX 包数） */
#define STAGE07_DEFAULT_NAPI_WEIGHT 32

/* 预分配 RX buffer 的大小（足够装下一个标准以太帧 + headroom） */
#define STAGE07_DEFAULT_RX_BUF_SIZE 2048

/* debugfs queue dump 时最多显示的 slot 数量（避免输出过长） */
#define STAGE07_MAX_QUEUE_DUMP 16

/*========================== slot 状态定义 ==========================*/

/*
 * stage07_buf_slot 的状态机：
 *
 *   FREE ──→ POSTED ──→ DONE ──→ FREE（循环）
 *     ↑                    │
 *     └────────────────────┘
 *
 * - FREE:       slot 空闲，可被 CPU 分配
 * - POSTED:     CPU 已投递 RX buffer，等待 device 写入数据
 * - SUBMITTED:  CPU 已提交 TX 请求，等待 device 处理
 * - DONE:       device 已处理完成，等待 CPU 回收（TX）或消费（RX）
 */
enum stage07_slot_state {
	S07_SLOT_FREE = 0,
	S07_SLOT_POSTED,    /* RX: CPU 预投递的 buffer，已准备好接收 */
	S07_SLOT_SUBMITTED, /* TX: CPU 已提交，等待 device 处理 */
	S07_SLOT_DONE,      /* TX+RX: device 已完成，等待 CPU 回收/消费 */
};

/*========================== 核心数据结构 ==========================*/

/*
 * stage07_desc — 描述符（descriptor）
 *
 * 【设计意图】
 * descriptor 是硬件队列的核心元素。在真实网卡中，它指向一块 DMA buffer。
 * 这里用纯软件数组模拟硬件 descriptor ring。
 *
 * 【字段解释】
 * - dma_addr:  buffer 的 DMA 地址（device 访问内存的钥匙）
 * - data_len:  实际数据长度
 * - state:     当前 slot 状态（FREE/POSTED/SUBMITTED/DONE）
 * - flags:     预留扩展（如 PAD / CSUM 等 offload 标记）
 */
struct stage07_desc {
	dma_addr_t dma_addr; /* DMA 地址，device 用它访问 buffer（TX→写，RX→读） */
	u32 data_len;         /* 数据长度（字节） */
	u16 state;            /* slot 状态，enum stage07_slot_state */
	u16 flags;            /* 预留：offload / checksum 等标记 */
};

/*
 * stage07_buf_slot — buffer + 元数据
 *
 * 【设计意图】
 * descriptor 只描述"地址"，真正装数据的是 skb（socket buffer）。
 * 这里把 skb 和它的 DMA 地址绑定在一起，方便统一管理。
 *
 * 【TX vs RX 的使用方式】
 * - TX: skb 是发送数据源，dma_addr 指向 skb->data
 * - RX: skb 是接收容器，dma_addr 指向 skb 的数据区
 */
struct stage07_buf_slot {
	struct sk_buff *skb;  /* socket buffer（TX=发送数据源，RX=接收容器） */
	dma_addr_t dma_addr;  /* skb->data 的 DMA 地址，device 用它访问内存 */
	u32 buf_len;          /* buffer 总大小（RX=rx_buf_size，TX=skb->len） */
	u32 data_len;         /* 实际数据长度（TX=skb->len，RX=device 写入后更新） */
	u16 state;            /* slot 状态，enum stage07_slot_state */
	u16 id;               /* slot 序号（用于 debug dump） */
};

/*
 * stage07_queue — TX 或 RX 队列
 *
 * 【设计意图】
 * 把 descriptor ring 和 buffer slot ring 打包在一起，形成完整的队列抽象。
 * TX 和 RX 各有一个独立的 stage07_queue。
 *
 * 【TX 3-index 模型】
 * - submit_idx:   CPU 提交新 TX 请求的位置（推进：CPU xmit）
 * - notify_idx:   backend 已消费到哪个位置（推进：backend kick）
 * - complete_idx: CPU 已回收 TX slot 的位置（推进：NAPI poll）
 *
 * 【RX 3-index 模型】
 * - post_idx:     CPU 补充新 RX buffer 的位置（推进：refill）
 * - device_idx:   backend 写入 RX buffer 的位置（推进：backend kick）
 * - consume_idx:  CPU 消费已完成 RX 包的位置（推进：NAPI poll）
 *
 * 【关键不变量】
 * - TX:  submit_idx >= notify_idx >= complete_idx（不等式可判断队列是否空/满）
 * - RX:  post_idx >= device_idx >= consume_idx（不等式同理）
 */
struct stage07_queue {
	struct stage07_desc *desc;       /* descriptor ring（硬件队列的软件模拟） */
	struct stage07_buf_slot *slots;  /* buffer slot ring（skb + DMA 绑定） */
	u16 size;                         /* queue 深度（= ring_size） */

	/* TX indexes */
	u16 submit_idx;   /* CPU 提交 TX 请求：xmit 调用时推进 */
	u16 notify_idx;   /* backend 消费 TX：kick_device 中推进 */
	u16 complete_idx; /* CPU 回收 TX：NAPI poll 中推进 */

	/* RX indexes */
	u16 post_idx;     /* CPU 补充 RX buffer：refill 时推进 */
	u16 device_idx;   /* backend 写入 RX：kick_device 中推进 */
	u16 consume_idx;  /* CPU 消费 RX：NAPI poll 中推进 */

	/* 保护多 CPU 并发访问 */
	spinlock_t lock;
};

/*
 * stage07_priv — 驱动私有数据
 *
 * 【在驱动中的位置】
 * net_device 通过 netdev_priv() 从 ndev->ml_priv 访问它。
 * 每个 netdev 实例有一个 stage07_priv 实例。
 *
 * 【字段分组】
 * 1. 基础结构：ndev / napi / debugfs_dir
 * 2. 队列：txq / rxq（各是一个完整的 queue）
 * 3. 运行时状态：rx_buf_size / tx_inflight / irq_masked 等
 * 4. 统计计数：TX / RX / NAPI / device 各一组 atomic64_t
 */
struct stage07_priv {
	struct net_device *ndev;     /* 关联的 net_device */
	struct napi_struct napi;    /* NAPI poll 结构（schedule 到此 napi） */
	struct dentry *dbg_dir;     /* debugfs 根目录（stats 和 queues 文件） */

	/* 驱动全局状态锁（保护 irq_masked 等状态） */
	spinlock_t state_lock;

	/*
	 * irq_masked — NAPI 模式下中断屏蔽标志
	 *
	 * NAPI 进入轮询时屏蔽设备中断，防止频繁 irq 风暴。
	 * NAPI complete 后重置为 false，允许下次 irq 重新触发 schedule。
	 *
	 * 【为什么不直接用 localirq_disable？】
	 * 因为 NAPI 的 schedule 是在 softirq 上下文中，禁用硬件中断不能阻止它。
	 * 需要软状态来追踪"当前 NAPI 是否在运行"。
	 */
	bool irq_masked;

	/* TX 和 RX 队列（各自独立的 queue 结构） */
	struct stage07_queue txq;
	struct stage07_queue rxq;

	/* 运行时参数 */
	u32 rx_buf_size; /* 预分配 RX buffer 大小 */

	/*
	 * tx_inflight — 飞行中的 TX 请求数量
	 *
	 * 【含义】已 submit 但尚未 complete 的 TX slot 数量。
	 * 【作用】用于判断 queue 是否已满（>= ring_size 时 stop queue）。
	 * 【更新时机】
	 *   - xmit: tx_inflight++
	 *   - NAPI complete_tx_one: tx_inflight--
	 */
	u16 tx_inflight;

	/*
	 * tx_done — backend 已处理完但 CPU 尚未回收的 TX slot 数量
	 *
	 * 【含义】device 已标记 DONE，但 NAPI 还未回收的 slot 数。
	 * 【作用】用于 NAPI poll 判断是否有 TX work 可做。
	 */
	u16 tx_done;

	/*
	 * rx_posted — CPU 已投递但 device 尚未写入的 RX slot 数量
	 *
	 * 【含义】已 POSTED 的 slot 等待 device DMA 写入。
	 * 【作用】backend kick 时检查"是否有可用 RX buffer"。
	 */
	u16 rx_posted;

	/*
	 * rx_ready — device 已写入数据、等待 CPU 消费的 RX slot 数量
	 *
	 * 【含义】DONE 状态的 RX slot，NAPI 可直接消费。
	 * 【作用】NAPI poll 判断是否有 RX work 可做。
	 */
	u16 rx_ready;

	/*========================== TX 统计 ==========================*/
	atomic64_t open_count;        /* netdev open 次数 */
	atomic64_t stop_count;       /* netdev stop 次数 */

	atomic64_t tx_submit_count;   /* xmit 被调用次数（= 收到的 TX 包数） */
	atomic64_t tx_complete_count; /* NAPI 回收 TX slot 的次数 */
	atomic64_t tx_packets;        /* 成功发送的包数 */
	atomic64_t tx_bytes;          /* 成功发送的字节数 */
	atomic64_t tx_dropped;        /* xmit 中丢弃的包数 */
	atomic64_t tx_busy;           /* 因 queue full 返回 NETDEV_TX_BUSY 的次数 */
	atomic64_t tx_linearize_count;/* skb_linearize 调用次数（非线性 skb 被重组） */
	atomic64_t tx_dma_map_ok;     /* DMA map 成功次数 */
	atomic64_t tx_dma_map_fail;   /* DMA map 失败次数 */
	atomic64_t tx_dma_unmap;      /* DMA unmap 次数（= complete_count） */

	/*========================== RX 统计 ==========================*/
	atomic64_t rx_post_count;     /* CPU 投递 RX buffer 的总次数（含 refill） */
	atomic64_t rx_consume_count;  /* NAPI 消费已完成 RX 包的次数 */
	atomic64_t rx_refill_count;   /* refill 触发总次数 */
	atomic64_t rx_packets;        /* 上送协议栈的 RX 包数 */
	atomic64_t rx_bytes;          /* 上送协议栈的 RX 字节数 */
	atomic64_t rx_dropped;        /* RX 路径丢弃的包数 */
	atomic64_t rx_dma_map_ok;     /* RX DMA map 成功次数 */
	atomic64_t rx_dma_map_fail;   /* RX DMA map 失败次数 */
	atomic64_t rx_dma_unmap;      /* RX DMA unmap 次数 */
	atomic64_t rx_truncated;     /* RX 数据被截断次数（copy_len < data_len） */
	atomic64_t rx_no_posted;      /* backend 因无可用 posted buffer 而等待的次数 */

	/*========================== NAPI / IRQ 统计 ==========================*/
	atomic64_t irq_count;         /* IRQ 触发总次数 */
	atomic64_t irq_mask_count;    /* IRQ 被屏蔽（NAPI 进入轮询）的次数 */
	atomic64_t irq_unmask_count;  /* IRQ 恢复（NAPI 退出轮询）的次数 */
	atomic64_t napi_schedule_count;/* napi_schedule() 调用次数 */
	atomic64_t napi_poll_count;   /* NAPI poll 被调用的次数 */
	atomic64_t napi_complete_count;/* NAPI complete 的次数 */
	atomic64_t napi_budget_exhaust_count;/* poll 耗尽 budget 的次数 */
	atomic64_t napi_work_total;   /* NAPI 总共处理的工作量（包数） */

	/*========================== 队列状态统计 ==========================*/
	atomic64_t ring_full_count;   /* 队列满的次数（xmit 返回 BUSY） */
	atomic64_t ring_empty_count;  /* 队列空的次数 */
	atomic64_t device_notify_count;/* stage07_kick_device() 被调用次数 */
	atomic64_t device_tx_processed;/* backend 处理 TX slot 的次数 */
	atomic64_t device_rx_produced;/* backend 写入 RX buffer 的次数 */

	/*========================== 最后包信息（调试用） ==========================*/
	atomic64_t last_tx_len;       /* 最后一次 TX 的长度 */
	atomic64_t last_tx_proto;     /* 最后一次 TX 的 ethertype（网络序） */
	atomic64_t last_rx_len;       /* 最后一次 RX 的长度 */
	atomic64_t last_rx_proto;     /* 最后一次 RX 的 ethertype（网络序） */
};

/*========================== 模块参数 ==========================*/

/* 驱动名称，/sys/module/netdev_stage07/parameters/ifname */
static char ifname[IFNAMSIZ] = "nds7";
module_param_string(ifname, ifname, sizeof(ifname), 0644);
MODULE_PARM_DESC(ifname, "interface name for stage07 real queue model");

/* TX/RX queue 深度（必须是 2 的幂） */
static int ring_size = STAGE07_DEFAULT_RING_SIZE;
module_param(ring_size, int, 0644);
MODULE_PARM_DESC(ring_size, "TX/RX queue depth");

/* NAPI poll weight（每轮 poll 最多处理的 RX 包数） */
static int napi_weight = STAGE07_DEFAULT_NAPI_WEIGHT;
module_param(napi_weight, int, 0644);
MODULE_PARM_DESC(napi_weight, "NAPI poll weight");

/* 预分配 RX buffer 大小 */
static int rx_buf_size = STAGE07_DEFAULT_RX_BUF_SIZE;
module_param(rx_buf_size, int, 0644);
MODULE_PARM_DESC(rx_buf_size, "RX buffer size for posted RX slots");

/*========================== 全局变量 ==========================*/

/* 模块级别的 net_device 指针（stage07 只支持一个实例） */
static struct net_device *stage07_dev;

/*========================== 工具函数 ==========================*/

/*
 * stage07_next_idx — ring index 推进（支持 wrap-around）
 *
 * 【参数】
 * - idx: 当前 index
 * - size: ring 大小（必须是 2 的幂）
 *
 * 【原理】
 * 使用模运算 (idx + 1) % size 实现环形回绕。
 * 因为 size 是 2 的幂，可以用位掩码优化：idx + 1 & (size - 1)
 *
 * 【为什么不用 ++idx % size？】
 * % 运算在某些架构上比位运算慢；但更重要的是，
 * 当 size 不是 2 的幂时，% 的行为是正确的，++ 方案则不通用。
 */
static inline u16 stage07_next_idx(u16 idx, u16 size)
{
	return (u16)((idx + 1) % size);
}

/*
 * stage07_state_name — 把 slot state 转换为可读字符串
 *
 * 【用途】
 * 用于 debugfs dump，把数字 state 输出成 FREE/POSTED/SUBMITTED/DONE。
 * 在 dmesg 和 queue dump 中可以看到这些字符串。
 */
static const char *stage07_state_name(u16 state)
{
	switch (state) {
	case S07_SLOT_FREE:      return "FREE";
	case S07_SLOT_POSTED:    return "POSTED";
	case S07_SLOT_SUBMITTED: return "SUBMITTED";
	case S07_SLOT_DONE:      return "DONE";
	default:                 return "?";
	}
}

/*========================== DMA 能力准备 ==========================*/

/*
 * stage07_prepare_dma_caps — 设置设备的 DMA 能力
 *
 * 【调用时机】
 * 在 alloc_netdev 之后、register_netdev 之前调用。
 *
 * 【操作】
 * 1. 设置 coherent_dma_mask = 64bit（保证一致映射 DMA 可用）
 * 2. 设置 dma_mask = 64bit（保证 streaming DMA 可用）
 * 3. 尝试设置 64bit mask，失败则回退到 32bit
 *
 * 【为什么 64→32 降级？】
 * 某些虚拟机（VMware Fusion / VirtualBox）的 PCI 虚拟设备只支持 32bit DMA。
 * 降级保证兼容性，但性能会受限。
 */
static int stage07_prepare_dma_caps(struct net_device *ndev)
{
	int ret;

	ndev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	ndev->dev.dma_mask = &ndev->dev.coherent_dma_mask;
	ret = dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(64));
	if (ret)
		ret = dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(32));
	return ret;
}

/*========================== 中断触发（IRQ 层） ==========================*/

/*
 * stage07_raise_irq — 触发软中断，启动 NAPI 轮询
 *
 * 【设计原则：IRQ 只负责叫醒，不做任何数据路径工作】
 *
 * 这个函数是真实网卡"中断处理函数"的教学模拟。
 * 真实网卡的中断处理函数（ISR）通常只做：
 *   1. ack 中断（清除 interrupt pending 位）
 *   2. 关闭本设备中断（防止风暴）
 *   3. schedule NAPI（启动轮询）
 *
 * 【本函数的工作】
 * 1. 检查 irq_masked（防止重复 schedule）
 * 2. 置 irq_masked = true（进入 NAPI 模式）
 * 3. 增加 irq_count / irq_mask_count 统计
 * 4. 调用 napi_schedule() 启动 NAPI softirq
 *
 * 【为什么不直接在这里处理 TX/RX？】
 * - 中断上下文不能睡眠（不能做 DMA map/unmap 等可能睡眠的操作）
 * - 中断上下文不能持有复杂锁（会阻塞其他中断）
 * - 批处理比每个包一次中断更高效（NAPI 的核心思想）
 */
static void stage07_raise_irq(struct stage07_priv *priv)
{
	bool do_schedule = false;
	unsigned long flags;

	/*
	 * irq_masked 双重检查：
	 * - 第一个检查在锁外（快速路径，无竞争时直接返回）
	 * - 第二个检查在锁内（防止多 CPU 并发进入）
	 */
	spin_lock_irqsave(&priv->state_lock, flags);
	if (!priv->irq_masked) {
		priv->irq_masked = true;  /* 标记"正在 NAPI"，防止重复触发 */
		do_schedule = true;
	}
	spin_unlock_irqrestore(&priv->state_lock, flags);

	if (!do_schedule)
		return;  /* 已经在 NAPI 模式，不再重复 schedule */

	atomic64_inc(&priv->irq_count);            /* 统计：irq 触发次数 */
	atomic64_inc(&priv->irq_mask_count);      /* 统计：进入 NAPI 模式次数 */
	atomic64_inc(&priv->napi_schedule_count);  /* 统计：schedule 次数 */
	napi_schedule(&priv->napi);                 /* 启动 NAPI softirq */
}

/*========================== RX Buffer 管理 ==========================*/

/*
 * stage07_post_rx_slot — 向 RX queue 投递一个空闲 buffer
 *
 * 【调用时机】
 * 1. 初始化时填充 RX queue（stage07_alloc_queues 中调用 128 次）
 * 2. NAPI poll 消费完一个 RX slot 后，refill 补充
 *
 * 【操作流程】
 * 1. 分配 skb（带 IP 对齐的 headroom）
 * 2. DMA map skb->data（建立 device 可访问的 DMA 地址）
 * 3. DMA sync（确保 device 在写入前可以看到最新数据）
 * 4. 填入 RX slot 和 RX desc
 * 5. 推进 post_idx
 * 6. rx_posted++
 *
 * 【返回值】
 * 0: 成功
 * -ENOMEM: skb 分配失败
 * -EIO: DMA map 失败
 */
static int stage07_post_rx_slot(struct stage07_priv *priv, u16 idx)
{
	struct stage07_buf_slot *slot = &priv->rxq.slots[idx];
	struct stage07_desc *desc = &priv->rxq.desc[idx];
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	unsigned long flags;

	/* 分配 skb：NET_IP_ALIGN 让 IP 头 4 字节对齐（性能优化） */
	skb = netdev_alloc_skb_ip_align(priv->ndev, priv->rx_buf_size);
	if (!skb) {
		atomic64_inc(&priv->rx_dma_map_fail);
		return -ENOMEM;
	}

	/*
	 * DMA map（streaming DMA，device 从 memory 读数据）
	 * - DMA_FROM_DEVICE: direction 是 device→memory（RX 场景）
	 * - 如果 DMA map 失败，释放 skb 并返回错误
	 */
	dma_addr = dma_map_single(&priv->ndev->dev, skb->data, priv->rx_buf_size,
				  DMA_FROM_DEVICE);
	if (dma_mapping_error(&priv->ndev->dev, dma_addr)) {
		dev_kfree_skb_any(skb);
		atomic64_inc(&priv->rx_dma_map_fail);
		return -EIO;
	}

	/*
	 * DMA sync for device：
	 * 确保在 device DMA 写入之前，cache 中的数据已刷新到 memory。
	 * 这对一致性 DMA（coherent）通常不需要，但对 streaming DMA 必须做。
	 */
	dma_sync_single_for_device(&priv->ndev->dev, dma_addr, priv->rx_buf_size,
				   DMA_FROM_DEVICE);
	atomic64_inc(&priv->rx_dma_map_ok);

	/* 更新 slot 和 desc，标记为 POSTED 状态 */
	spin_lock_irqsave(&priv->state_lock, flags);
	slot->skb = skb;
	slot->dma_addr = dma_addr;
	slot->buf_len = priv->rx_buf_size;
	slot->data_len = 0;   /* 尚未写入数据，长度为 0 */
	slot->state = S07_SLOT_POSTED;
	desc->dma_addr = dma_addr;
	desc->data_len = 0;
	desc->state = S07_SLOT_POSTED;
	priv->rx_posted++;
	priv->rxq.post_idx = stage07_next_idx(idx, priv->rxq.size); /* 推进 post 指针 */
	spin_unlock_irqrestore(&priv->state_lock, flags);

	atomic64_inc(&priv->rx_post_count);
	atomic64_inc(&priv->rx_refill_count);
	return 0;
}

/*
 * stage07_refill_one — 尝试 refill 一个 RX slot
 *
 * 【设计意图】
 * 在 RX queue 中找一个 FREE 的 slot，调用 stage07_post_rx_slot 填充它。
 * 这是 RX replenishment 的核心逻辑。
 *
 * 【被调用场景】
 * - NAPI poll 的 stage07_consume_rx_one() 中，每消费一个 DONE slot 就触发一次
 * - 保证 RX slot 被及时补充
 *
 * 【返回值】
 * 0: 成功 refill 一个 slot
 * -EBUSY: post_idx 指向的 slot 不是 FREE 状态（队列已满，refill 失败）
 *
 * 【关键检查】
 * slot->state != S07_SLOT_FREE 说明该 slot 还在使用中（已被 POSTED 或 DONE），
 * 此时不能 refill，等 NAPI 消费完它之后自然变回 FREE。
 */
static int stage07_refill_one(struct stage07_priv *priv)
{
	u16 idx;
	unsigned long flags;

	/*
	 * 检查 post_idx 指向的 slot 是否 FREE：
	 * 如果不是 FREE，说明队列已满（所有 slot 都在使用中），refill 失败。
	 */
	spin_lock_irqsave(&priv->state_lock, flags);
	idx = priv->rxq.post_idx;
	if (priv->rxq.slots[idx].state != S07_SLOT_FREE) {
		spin_unlock_irqrestore(&priv->state_lock, flags);
		atomic64_inc(&priv->ring_full_count); /* 队列满统计 */
		return -EBUSY;
	}
	spin_unlock_irqrestore(&priv->state_lock, flags);

	return stage07_post_rx_slot(priv, idx);
}

/*
 * stage07_reset_queue_state — 重置队列 index 和状态
 *
 * 【调用时机】
 * - 模块加载时，在 queue 分配后调用一次
 *
 * 【重置内容】
 * 所有 TX index → 0，所有 RX index → 0
 * tx_inflight / tx_done / rx_posted / rx_ready → 0
 * irq_masked → false（允许 irq 触发）
 */
static void stage07_reset_queue_state(struct stage07_priv *priv)
{
	unsigned long flags;

	spin_lock_irqsave(&priv->state_lock, flags);
	priv->txq.submit_idx = 0;
	priv->txq.notify_idx = 0;
	priv->txq.complete_idx = 0;
	priv->rxq.post_idx = 0;
	priv->rxq.device_idx = 0;
	priv->rxq.consume_idx = 0;
	priv->tx_inflight = 0;
	priv->tx_done = 0;
	priv->rx_posted = 0;
	priv->rx_ready = 0;
	priv->irq_masked = false;
	spin_unlock_irqrestore(&priv->state_lock, flags);
}

/*========================== Backend 模拟（device 侧） ==========================*/

/*
 * stage07_kick_device — 通知 backend（device）处理 TX/RX queue
 *
 * 【设计意图】
 * 这是本驱动最关键的教学模拟点：
 * - 真实网卡中，CPU 写 PCI doorbell → device 中断 → device 处理队列
 * - stage07 中，直接调用这个函数（同步 memcpy 模拟 device DMA）
 *
 * 【在真实 virtio-net 中的对应】
 * - 这个函数对应 vhost-net 的 TX/RX 处理循环
 * - TX notify → avail ring kick → vhost 读取 buffer → DMA 复制
 * - RX notify → vhost 写入 RX buffer → used ring 更新 → 触发中断
 *
 * 【TX 处理流程】
 * 1. 取 notify_idx 位置的 SUBMITTED slot
 * 2. 检查：desc 和 slot 都是 SUBMITTED 状态
 * 3. 标记为 DONE
 * 4. notify_idx++（backend 消费进度）
 * 5. tx_done++（待 NAPI 回收）
 *
 * 【RX 处理流程】
 * 1. 检查 rx_posted > 0（有可用 buffer）
 * 2. 取 device_idx 位置的 POSTED slot
 * 3. memcpy(TX skb data → RX skb data)（模拟 device DMA 写入）
 * 4. 更新 data_len = copy_len
 * 5. 标记为 DONE
 * 6. device_idx++（backend 写入进度）
 * 7. rx_posted--（减少可用 buffer 计数）
 * 8. rx_ready++（增加待消费计数）
 *
 * 【返回值】
 * 如果有 TX DONE 或 RX DONE 产生，调用 stage07_raise_irq() 触发 NAPI。
 */
static void stage07_kick_device(struct stage07_priv *priv)
{
	bool produced = false;  /* 标记"是否有 DONE 事件需要通知 CPU" */
	unsigned long flags;

	atomic64_inc(&priv->device_notify_count); /* 统计：kick 调用次数 */

	/*
	 * TX + RX 处理都在 state_lock 保护下进行。
	 * 因为 backend 处理和 CPU NAPI poll 会并发访问 queue。
	 *
	 * 【为什么 TX 和 RX 在同一个循环里？】
	 * 在本驱动中，TX 和 RX 是独立的（不是 stage04 里 TX 复用 RX buffer）。
	 * 这里把 TX/RX 处理放在一起是为了模拟"批量中断处理"的概念。
	 * 真实驱动中，TX completions 和 RX completions 可能是分开处理的。
	 */
	spin_lock_irqsave(&priv->state_lock, flags);
	while (priv->txq.notify_idx != priv->txq.submit_idx && priv->rx_posted > 0) {
		struct stage07_desc *txd = &priv->txq.desc[priv->txq.notify_idx];
		struct stage07_buf_slot *txs = &priv->txq.slots[priv->txq.notify_idx];
		struct stage07_desc *rxd = &priv->rxq.desc[priv->rxq.device_idx];
		struct stage07_buf_slot *rxs = &priv->rxq.slots[priv->rxq.device_idx];
		u32 copy_len;

		/*
		 * 边界检查：确保 TX slot 确实是 SUBMITTED 状态。
		 * 如果不是，说明状态异常（可能是 CPU 和 backend 并发导致），跳过。
		 */
		if (txd->state != S07_SLOT_SUBMITTED || txs->state != S07_SLOT_SUBMITTED)
			break;

		/*
		 * 边界检查：确保 RX slot 确实是 POSTED 状态且有 skb。
		 * rx_posted > 0 是全局检查，具体 slot 仍需二次确认。
		 */
		if (rxd->state != S07_SLOT_POSTED || rxs->state != S07_SLOT_POSTED || !rxs->skb)
			break;

		/*
		 * 计算拷贝长度：取 TX 长度和 RX buffer 长度的较小值。
		 * 如果 TX 数据比 RX buffer 大，会截断（rx_truncated++ 统计）。
		 */
		copy_len = min_t(u32, txd->data_len, rxs->buf_len);
		if (copy_len < txd->data_len)
			atomic64_inc(&priv->rx_truncated);

		/*
		 * DMA sync for device：
		 * 在 memcpy 之前，确保 RX buffer 对 device 是可写的。
		 */
		dma_sync_single_for_device(&priv->ndev->dev, rxs->dma_addr, rxs->buf_len,
					   DMA_FROM_DEVICE);

		/*
		 * 【核心操作：memcpy 模拟 device DMA copy】
		 *
		 * stage04 的做法：TX 直接写入 POSTED RX buffer
		 * stage07 的做法：TX 和 RX 完全独立，这里显式 memcpy
		 *
		 * 这个 memcpy 在真实 virtio-net 中对应：
		 * vhost-net 的 DMA 复制操作（skb data → guest memory）
		 */
		memcpy(rxs->skb->data, txs->skb->data, copy_len);

		/*
		 * DMA sync for CPU：
		 * memcpy 完成后，CPU 要能读到数据，需要 invalidate cache。
		 */
		dma_sync_single_for_cpu(&priv->ndev->dev, rxs->dma_addr, copy_len,
					DMA_FROM_DEVICE);

		/* 更新 RX slot：记录实际写入的数据长度，标记为 DONE */
		rxs->data_len = copy_len;
		rxs->state = S07_SLOT_DONE;
		rxd->data_len = copy_len;
		rxd->state = S07_SLOT_DONE;

		/* 更新 RX 计数：rx_posted--（buffer 已填），rx_ready++（可消费） */
		priv->rx_ready++;
		if (priv->rx_posted > 0)
			priv->rx_posted--;

		/* 推进 device_idx（backend 写入进度） */
		priv->rxq.device_idx = stage07_next_idx(priv->rxq.device_idx, priv->rxq.size);
		atomic64_inc(&priv->device_rx_produced); /* 统计：backend 产生 RX 包数 */

		/* 更新 TX slot：标记为 DONE */
		txs->state = S07_SLOT_DONE;
		txd->state = S07_SLOT_DONE;

		/* 更新 TX 计数：tx_done++（backend 已处理，待 NAPI 回收） */
		priv->tx_done++;

		/* 推进 notify_idx（backend 消费进度） */
		priv->txq.notify_idx = stage07_next_idx(priv->txq.notify_idx, priv->txq.size);
		atomic64_inc(&priv->device_tx_processed); /* 统计：backend 处理 TX 数 */

		produced = true; /* 至少处理了一组，标记需要触发 IRQ */
	}

	/*
	 * rx_no_posted 统计：
	 * TX queue 里还有未处理的请求，但 RX 没有可用的 posted buffer。
	 * 这说明 refill 不够快，需要增加 RX buffer 数量或加快 refill 速度。
	 */
	if (priv->txq.notify_idx != priv->txq.submit_idx && priv->rx_posted == 0)
		atomic64_inc(&priv->rx_no_posted);
	spin_unlock_irqrestore(&priv->state_lock, flags);

	/*
	 * 如果产生了 DONE 事件（TX DONE 或 RX DONE），触发 IRQ 通知 CPU。
	 * 注意：raised_irq 内部会检查 irq_masked，避免重复 schedule。
	 */
	if (produced)
		stage07_raise_irq(priv);
}

/*========================== TX 完成处理（NAPI 层） ==========================*/

/*
 * stage07_complete_tx_one — NAPI poll 中回收一个 TX slot
 *
 * 【调用时机】
 * NAPI poll 的 stage07_poll() 中，在处理 RX 之前先回收 TX done slots。
 *
 * 【操作流程】
 * 1. 检查 tx_done > 0（有待回收的 slot）
 * 2. 取 complete_idx 位置的 DONE slot
 * 3. 保存 slot 信息（skb / dma_addr / data_len）
 * 4. 清理 slot（skb=NULL, state=FREE）
 * 5. DMA unmap（释放 DMA 地址）
 * 6. dev_consume_skb_any()（归还 skb 到系统）
 * 7. 推进 complete_idx
 * 8. tx_inflight--（飞行计数减一）
 * 9. 如果 queue 之前被 stop，现在 wake 回来
 *
 * 【返回值】
 * 1: 成功回收一个 slot
 * 0: 没有可回收的 slot（tx_done == 0 或 slot 状态不对）
 *
 * 【关键设计】
 * - TX 完成和 RX 消费在同一个 poll 调用中分开处理
 * - 这样可以保证 TX 完成优先被处理（释放 TX queue 空间）
 */
static int stage07_complete_tx_one(struct stage07_priv *priv)
{
	struct stage07_buf_slot saved = { 0 }; /* 保存 slot 信息的临时变量 */
	u16 idx;
	unsigned long flags;

	/* 检查是否有 tx_done > 0（backend 已处理但 CPU 未回收） */
	spin_lock_irqsave(&priv->state_lock, flags);
	if (!priv->tx_done) {
		spin_unlock_irqrestore(&priv->state_lock, flags);
		return 0;
	}

	idx = priv->txq.complete_idx;

	/*
	 * 二次检查 slot 状态：
	 * 如果 slot 不是 DONE 状态，说明 backend 还没处理完，跳过。
	 */
	if (priv->txq.desc[idx].state != S07_SLOT_DONE ||
	    priv->txq.slots[idx].state != S07_SLOT_DONE ||
	    !priv->txq.slots[idx].skb) {
		spin_unlock_irqrestore(&priv->state_lock, flags);
		return 0;
	}

	/* 保存 slot 信息到栈变量（因为下面要清零 slot） */
	saved = priv->txq.slots[idx];

	/* 清零 slot，回收给 queue */
	priv->txq.slots[idx].skb = NULL;
	priv->txq.slots[idx].dma_addr = 0;
	priv->txq.slots[idx].buf_len = 0;
	priv->txq.slots[idx].data_len = 0;
	priv->txq.slots[idx].state = S07_SLOT_FREE;
	priv->txq.desc[idx].dma_addr = 0;
	priv->txq.desc[idx].data_len = 0;
	priv->txq.desc[idx].state = S07_SLOT_FREE;

	/* 推进 complete_idx（CPU 回收进度） */
	priv->txq.complete_idx = stage07_next_idx(priv->txq.complete_idx, priv->txq.size);

	/* 更新计数 */
	priv->tx_done--;
	if (priv->tx_inflight > 0)
		priv->tx_inflight--;
	spin_unlock_irqrestore(&priv->state_lock, flags);

	/*
	 * DMA unmap：释放之前映射的 DMA 地址。
	 * 注意 direction 是 DMA_TO_DEVICE（TX 场景）。
	 */
	dma_unmap_single(&priv->ndev->dev, saved.dma_addr, saved.data_len, DMA_TO_DEVICE);
	atomic64_inc(&priv->tx_dma_unmap);      /* 统计：DMA unmap 次数 */
	atomic64_inc(&priv->tx_complete_count);   /* 统计：TX 完成次数 */

	/* 归还 skb：dev_consume_skb_any 会直接 kfree_skb（不经过 GSO/TSO 处理） */
	dev_consume_skb_any(saved.skb);

	/*
	 * 如果 queue 之前因满被 stop，现在 TX slot 已回收，应该 wake 回来。
	 * netif_wake_queue() 会触发 netdev 运行队列再次调用 ndo_start_xmit()。
	 */
	if (netif_queue_stopped(priv->ndev))
		netif_wake_queue(priv->ndev);
	return 1;
}

/*========================== RX 消费处理（NAPI 层） ==========================*/

/*
 * stage07_consume_rx_one — NAPI poll 中消费一个完成的 RX 包
 *
 * 【调用时机】
 * stage07_poll() 中，在 TX 完成回收之后，按 budget 限制逐个消费 RX。
 *
 * 【操作流程】
 * 1. 检查 rx_ready > 0（有 device 写入的 RX 包等待消费）
 * 2. 取 consume_idx 位置的 DONE slot
 * 3. DMA unmap skb->data
 * 4. skb_put() 设置正确的数据长度
 * 5. eth_type_trans() 解析以太网帧类型
 * 6. netif_receive_skb() 送上 Linux 协议栈
 * 7. 触发 refill 补充一个空闲 slot
 *
 * 【返回值】
 * 1: 成功消费一个 RX 包
 * 0: 没有可消费的 RX 包
 *
 * 【eth_type_trans 的作用】
 * 解析 skb 的 MAC 头，确定协议类型（IPv4/IPv6/ARP 等），
 * 并设置 skb->protocol 和 skb->dev 为本设备。
 * Linux 协议栈根据 protocol 找到对应的 packet_type handler 进行下一步处理。
 *
 * 【与 stage04 的区别】
 * stage04: consume 和 refill 在同一个循环里
 * stage07: consume 触发 refill，refill 计数独立统计（rx_refill_count）
 */
static int stage07_consume_rx_one(struct stage07_priv *priv)
{
	struct stage07_buf_slot saved = { 0 };
	u16 idx;
	__be16 proto;
	unsigned long flags;
	int rc;

	/* 检查是否有 rx_ready > 0（device 已写入，等待 CPU 消费） */
	spin_lock_irqsave(&priv->state_lock, flags);
	if (!priv->rx_ready) {
		spin_unlock_irqrestore(&priv->state_lock, flags);
		return 0;
	}

	idx = priv->rxq.consume_idx;

	/* 二次检查 slot 状态 */
	if (priv->rxq.desc[idx].state != S07_SLOT_DONE ||
	    priv->rxq.slots[idx].state != S07_SLOT_DONE ||
	    !priv->rxq.slots[idx].skb) {
		spin_unlock_irqrestore(&priv->state_lock, flags);
		return 0;
	}

	/* 保存 slot 信息 */
	saved = priv->rxq.slots[idx];

	/* 清零 slot，回收给 queue */
	priv->rxq.slots[idx].skb = NULL;
	priv->rxq.slots[idx].dma_addr = 0;
	priv->rxq.slots[idx].buf_len = 0;
	priv->rxq.slots[idx].data_len = 0;
	priv->rxq.slots[idx].state = S07_SLOT_FREE;
	priv->rxq.desc[idx].dma_addr = 0;
	priv->rxq.desc[idx].data_len = 0;
	priv->rxq.desc[idx].state = S07_SLOT_FREE;

	/* 推进 consume_idx */
	priv->rxq.consume_idx = stage07_next_idx(priv->rxq.consume_idx, priv->rxq.size);
	if (priv->rx_ready > 0)
		priv->rx_ready--;
	spin_unlock_irqrestore(&priv->state_lock, flags);

	/* DMA unmap（RX direction: device → memory） */
	dma_unmap_single(&priv->ndev->dev, saved.dma_addr, saved.buf_len, DMA_FROM_DEVICE);
	atomic64_inc(&priv->rx_dma_unmap);

	/* skb_put: 设置 skb 的 data 和 tail 指针，扩展 skb 到实际数据长度 */
	skb_put(saved.skb, saved.data_len);

	/*
	 * eth_type_trans: 解析以太网帧头
	 * - 剥除 MAC 头
	 * - 设置 skb->protocol 为 EtherType（如 ETH_P_IP = 0x0800）
	 * - 设置 skb->dev = ndev
	 */
	proto = eth_type_trans(saved.skb, priv->ndev);

	/*
	 * 送上 Linux 协议栈：
	 * - netif_receive_skb() 是慢速路径（走 netfilter / routing）
	 * - 返回值 rc 被忽略（实际项目中应该检查是否为 NET_RX_SUCCESS）
	 */
	rc = netif_receive_skb(saved.skb);
	(void)rc;

	/* 更新统计 */
	atomic64_inc(&priv->rx_consume_count);
	atomic64_inc(&priv->rx_packets);
	atomic64_add(saved.data_len, &priv->rx_bytes);
	atomic64_set(&priv->last_rx_len, saved.data_len);
	atomic64_set(&priv->last_rx_proto, ntohs(proto));

	/*
	 * 消费完一个 RX slot 后，立即 refill 补充，保持 rx_posted 不空。
	 * refill 失败（队列满）会导致 rx_dropped++，但本驱动中队列足够大，不会发生。
	 */
	if (stage07_refill_one(priv))
		atomic64_inc(&priv->rx_dropped);
	return 1;
}

/*========================== NAPI 轮询主函数 ==========================*/

/*
 * stage07_poll — NAPI poll 函数（核心轮询处理）
 *
 * 【调用时机】
 * 1. stage07_raise_irq() 中 napi_schedule() 被调用后
 * 2. Linux softirq 调度器选择本 napi 执行
 *
 * 【budget 控制】
 * budget 参数来自 napi_weight（默认 32）。
 * 每轮 poll 最多处理 budget 个 RX 包，防止 poll 函数占用太久。
 *
 * 【操作流程】
 * 1. 先回收所有 TX done slots（不限 budget，因为 TX 完成释放内存）
 * 2. 按 budget 上限消费 RX done slots
 * 3. 判断是否需要继续轮询（more_rx）
 * 4. 如果无更多 RX，调用 napi_complete_done() 退出轮询
 * 5. 如果有剩余 TX done 但无 RX，重新触发 IRQ（NAPI 重入）
 *
 * 【napi_complete_done 的作用】
 * - 标记本轮 NAPI 结束
 * - 设置 irq_masked = false（允许下次 irq 触发）
 * - 如果 work_done < budget 且 no more work，返回 true（退出轮询模式）
 *
 * 【为什么先处理 TX 再处理 RX？】
 * TX slot 回收 = 释放内存 + 唤醒 netdev queue。
 * 优先回收 TX 可以更快地释放发送队列，让更多包进来。
 */
static int stage07_poll(struct napi_struct *napi, int budget)
{
	struct stage07_priv *priv = container_of(napi, struct stage07_priv, napi);
	int work_done = 0;       /* 本轮 poll 处理的 RX 包数 */
	bool budget_exhausted = false;
	bool more_rx = false;
	unsigned long flags;

	atomic64_inc(&priv->napi_poll_count); /* 统计：poll 调用次数 */

	/*
	 * 先回收所有 TX done slots（不限数量，TX 完成是免费的）
	 * 循环直到所有 tx_done 被处理完。
	 */
	while (stage07_complete_tx_one(priv))
		;

	/*
	 * 按 budget 限制消费 RX packets。
	 * work_done 计数用于判断是否耗尽 budget。
	 */
	while (work_done < budget && stage07_consume_rx_one(priv))
		work_done++;

	atomic64_add(work_done, &priv->napi_work_total); /* 累计工作量 */

	/* budget_exhausted 判断：如果恰好等于 budget，说明可能还有更多包 */
	if (work_done == budget) {
		budget_exhausted = true;
		atomic64_inc(&priv->napi_budget_exhaust_count);
	}

	/*
	 * more_rx 判断：rx_ready > 0 说明还有未处理的 RX 包。
	 * 如果有 more_rx，本轮 poll 结束后 Linux 可能会立即再次 schedule（poll 模式）。
	 * 如果没有 more_rx，调用 napi_complete_done() 退出 poll 模式。
	 */
	spin_lock_irqsave(&priv->state_lock, flags);
	more_rx = priv->rx_ready > 0;
	if (!more_rx) {
		/*
		 * napi_complete_done: 退出 NAPI 模式
		 * 内部会调用 netif_receive_skb() 的中断处理重新使能设备中断。
		 * 这里手动重置 irq_masked = false，允许下次 irq 触发。
		 */
		napi_complete_done(napi, work_done);
		priv->irq_masked = false;
	}
	spin_unlock_irqrestore(&priv->state_lock, flags);

	/* 统计：NAPI complete 次数 */
	if (!more_rx) {
		atomic64_inc(&priv->napi_complete_count);
		atomic64_inc(&priv->irq_unmask_count);
	}

	/*
	 * 边界情况：budget 用尽但仍有 tx_done，说明还有 TX 工作可以做。
	 * 重新 raise_irq 让下一轮 poll 处理。
	 * （这种情况在正常负载下很少见）
	 */
	if (!budget_exhausted && !more_rx && priv->tx_done)
		stage07_raise_irq(priv);

	return work_done; /* 返回处理了多少个 RX 包 */
}

/*========================== TX 发送（ndo_start_xmit） ==========================*/

/*
 * stage07_xmit — TX 发送函数（ndo_start_xmit 实现）
 *
 * 【调用时机】
 * Linux 协议栈（或用户程序通过 socket）需要发送一个包时，
 * netdev queue 调用此函数。
 *
 * 【操作流程】
 * 1. skb 非线性检查：如果 skb 有多个 fragment，调用 skb_linearize 重组
 * 2. DMA map skb->data（建立 device 可访问的 DMA 地址）
 * 3. 检查 tx_inflight >= ring_size（队列满）：stop queue，unmap，返回 BUSY
 * 4. 找一个 FREE 的 TX slot
 * 5. 填入 DMA 地址、数据长度、skb
 * 6. slot 状态设为 SUBMITTED
 * 7. submit_idx++（推进 CPU 提交指针）
 * 8. tx_inflight++（飞行计数增加）
 * 9. 调用 stage07_kick_device() 通知 backend
 *
 * 【返回值】
 * - NETDEV_TX_OK: 成功（slot 已提交，不等于发送完成）
 * - NETDEV_TX_BUSY: 队列满，稍后重试（调用者应该 stop queue 后重试）
 *
 * 【关键设计：TX 是异步的】
 * xmit 返回 NETDEV_TX_OK 只表示"包已入队"，不表示"包已发送"。
 * 实际发送由 backend（stage07_kick_device）同步处理，
 * 完成由 NAPI poll 异步回收。
 */
static netdev_tx_t stage07_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct stage07_priv *priv = netdev_priv(ndev);
	struct stage07_desc *txd;
	struct stage07_buf_slot *txs;
	dma_addr_t tx_dma;
	u16 idx;
	unsigned long flags;

	/*
	 * skb_nonlinear 检查：
	 * skb 可能由多个 page fragment 组成（scatter-gather list）。
	 * DMA map 只能映射连续内存，所以需要 skb_linearize 重组。
	 *
	 * skb_linearize 会分配新 buffer，拷贝所有 fragment 数据到新 buffer，
	 * 然后释放原 skb。这是一个可能的性能热点。
	 */
	if (unlikely(skb_is_nonlinear(skb))) {
		if (skb_linearize(skb)) {
			atomic64_inc(&priv->tx_dropped);
			dev_kfree_skb_any(skb);
			return NETDEV_TX_OK; /* 分配失败，丢弃包（但不 blocking） */
		}
		atomic64_inc(&priv->tx_linearize_count);
	}

	/*
	 * DMA map（streaming DMA，device 从 memory 读数据）
	 * direction: DMA_TO_DEVICE（TX 场景，device 读内存）
	 * 注意：这里只 map skb->data 部分，不是整个 skb。
	 */
	tx_dma = dma_map_single(&ndev->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(&ndev->dev, tx_dma)) {
		atomic64_inc(&priv->tx_dma_map_fail);
		atomic64_inc(&priv->tx_dropped);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	atomic64_inc(&priv->tx_dma_map_ok);
	dma_sync_single_for_device(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);

	/*
	 * 检查 tx_inflight >= ring_size：
	 * 如果 queue 已满，stop queue 并返回 BUSY。
	 * 调用者（netdev queue）会阻塞，直到 NAPI 回收 TX slot 后唤醒。
	 */
	spin_lock_irqsave(&priv->state_lock, flags);
	if (priv->tx_inflight >= priv->txq.size) {
		spin_unlock_irqrestore(&priv->state_lock, flags);
		dma_unmap_single(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);
		atomic64_inc(&priv->tx_dma_unmap);
		atomic64_inc(&priv->tx_busy);
		atomic64_inc(&priv->ring_full_count);
		netif_stop_queue(ndev); /* 暂停 netdev queue */
		return NETDEV_TX_BUSY;
	}

	/*
	 * 找 submit_idx 位置的 FREE slot。
	 * 如果 slot 不是 FREE 状态（可能未及时回收），也返回 BUSY。
	 */
	idx = priv->txq.submit_idx;
	txd = &priv->txq.desc[idx];
	txs = &priv->txq.slots[idx];
	if (txd->state != S07_SLOT_FREE || txs->state != S07_SLOT_FREE) {
		spin_unlock_irqrestore(&priv->state_lock, flags);
		dma_unmap_single(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);
		atomic64_inc(&priv->tx_dma_unmap);
		atomic64_inc(&priv->tx_busy);
		atomic64_inc(&priv->ring_full_count);
		return NETDEV_TX_BUSY;
	}

	/* 填入 TX slot 信息 */
	txd->dma_addr = tx_dma;
	txd->data_len = skb->len;
	txd->state = S07_SLOT_SUBMITTED; /* 标记为已提交，等待 backend 处理 */
	txs->skb = skb;
	txs->dma_addr = tx_dma;
	txs->buf_len = skb->len;
	txs->data_len = skb->len;
	txs->state = S07_SLOT_SUBMITTED;

	/* 推进 submit_idx，更新飞行计数 */
	priv->txq.submit_idx = stage07_next_idx(priv->txq.submit_idx, priv->txq.size);
	priv->tx_inflight++;
	spin_unlock_irqrestore(&priv->state_lock, flags);

	/* 更新统计 */
	atomic64_inc(&priv->tx_submit_count);
	atomic64_inc(&priv->tx_packets);
	atomic64_add(skb->len, &priv->tx_bytes);
	atomic64_set(&priv->last_tx_len, skb->len);
	atomic64_set(&priv->last_tx_proto, ntohs(skb->protocol));

	/*
	 * stage07_kick_device()：通知 backend（同步处理 TX + RX）
	 *
	 * 【关键点】
	 * 这里不是异步中断，而是直接函数调用。
	 * 真实网卡中，这是 CPU 写 PCI doorbell 触发 device 中断。
	 */
	stage07_kick_device(priv);
	return NETDEV_TX_OK;
}

/*========================== netdev 生命周期 ==========================*/

/*
 * stage07_open — netdev 打开（ifconfig nds7 up）
 *
 * 【调用时机】
 * 用户执行 ifconfig up 或 ip link set up 时调用。
 *
 * 【操作】
 * 1. napi_enable(&priv->napi)：使能 NAPI（允许 schedule）
 * 2. netif_start_queue(ndev)：启动 netdev 发送队列
 * 3. open_count++：统计
 *
 * 【与 stage07_stop 的关系】
 * stop 时调用 napi_disable() 和 netif_stop_queue()。
 * open/stop 可以多次调用（对应 ifconfig up/down）。
 */
static int stage07_open(struct net_device *ndev)
{
	struct stage07_priv *priv = netdev_priv(ndev);

	napi_enable(&priv->napi);
	netif_start_queue(ndev);
	atomic64_inc(&priv->open_count);
	return 0;
}

/*
 * stage07_stop — netdev 关闭（ifconfig nds7 down）
 *
 * 【调用时机】
 * 用户执行 ifconfig down 时调用。
 *
 * 【操作】
 * 1. netif_stop_queue(ndev)：暂停发送队列
 * 2. napi_disable(&priv->napi)：禁用 NAPI（禁止 schedule）
 * 3. stop_count++：统计
 *
 * 【资源清理】
 * stop 不清理 queue 内容（TX in-flight 包会继续完成）。
 * 真正清理在模块卸载（rmmod）时的 stage07_free_queues() 中。
 */
static int stage07_stop(struct net_device *ndev)
{
	struct stage07_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	napi_disable(&priv->napi);
	atomic64_inc(&priv->stop_count);
	return 0;
}

/*
 * stage07_get_stats64 — 获取网络设备统计（ip -s link show 调用）
 *
 * 【调用时机】
 * 用户执行 ip 或 ifconfig 查看统计时调用。
 *
 * 【实现】
 * 直接从 atomic64_t 计数器读取，返回 rtnl_link_stats64 结构。
 * 注意：这里不需要加锁，因为 atomic64_t 本身是原子的。
 */
static void stage07_get_stats64(struct net_device *ndev,
				struct rtnl_link_stats64 *stats)
{
	struct stage07_priv *priv = netdev_priv(ndev);

	stats->tx_packets = atomic64_read(&priv->tx_packets);
	stats->tx_bytes = atomic64_read(&priv->tx_bytes);
	stats->tx_dropped = atomic64_read(&priv->tx_dropped);
	stats->rx_packets = atomic64_read(&priv->rx_packets);
	stats->rx_bytes = atomic64_read(&priv->rx_bytes);
	stats->rx_dropped = atomic64_read(&priv->rx_dropped);
}

/* netdev_ops 表：绑定 ndo_start_xmit / ndo_open / ndo_stop / ndo_get_stats64 */
static const struct net_device_ops stage07_netdev_ops = {
	.ndo_open		= stage07_open,
	.ndo_stop		= stage07_stop,
	.ndo_start_xmit		= stage07_xmit,
	.ndo_get_stats64	= stage07_get_stats64,
};

/*========================== debugfs 统计 ==========================*/

/*
 * stage07_stats_show — debugfs stats 文件的 seq_file 读函数
 *
 * 【文件路径】
 * /sys/kernel/debug/netdev_stage07/stats
 *
 * 【输出内容】
 * 所有 TX / RX / NAPI / device 计数器。
 * 用于 smoke 测试验证和数据路径观测。
 *
 * 【seq_file 机制】
 * Linux 内核提供 seq_file 接口，方便生成/proc 或 debugfs 的迭代式输出。
 * 每次 read 调用，start/next/show/stop 四个函数按序执行。
 * 本驱动使用 single_open（适合小数据量的简单一次性输出）。
 */
static int stage07_stats_show(struct seq_file *m, void *v)
{
	struct net_device *ndev = m->private;
	struct stage07_priv *priv = netdev_priv(ndev);

	seq_printf(m, "ifname=%s\n", ndev->name);
	seq_printf(m, "ring_size=%u\n", priv->txq.size);
	seq_printf(m, "rx_buf_size=%u\n", priv->rx_buf_size);
	seq_printf(m, "tx_inflight=%u\n", priv->tx_inflight);
	seq_printf(m, "tx_done=%u\n", priv->tx_done);
	seq_printf(m, "rx_posted=%u\n", priv->rx_posted);
	seq_printf(m, "rx_ready=%u\n", priv->rx_ready);
	seq_printf(m, "open_count=%lld\n", atomic64_read(&priv->open_count));
	seq_printf(m, "stop_count=%lld\n", atomic64_read(&priv->stop_count));
	seq_printf(m, "tx_submit_count=%lld\n", atomic64_read(&priv->tx_submit_count));
	seq_printf(m, "tx_complete_count=%lld\n", atomic64_read(&priv->tx_complete_count));
	seq_printf(m, "tx_packets=%lld\n", atomic64_read(&priv->tx_packets));
	seq_printf(m, "tx_bytes=%lld\n", atomic64_read(&priv->tx_bytes));
	seq_printf(m, "tx_dropped=%lld\n", atomic64_read(&priv->tx_dropped));
	seq_printf(m, "tx_busy=%lld\n", atomic64_read(&priv->tx_busy));
	seq_printf(m, "tx_linearize_count=%lld\n", atomic64_read(&priv->tx_linearize_count));
	seq_printf(m, "tx_dma_map_ok=%lld\n", atomic64_read(&priv->tx_dma_map_ok));
	seq_printf(m, "tx_dma_map_fail=%lld\n", atomic64_read(&priv->tx_dma_map_fail));
	seq_printf(m, "tx_dma_unmap=%lld\n", atomic64_read(&priv->tx_dma_unmap));
	seq_printf(m, "rx_post_count=%lld\n", atomic64_read(&priv->rx_post_count));
	seq_printf(m, "rx_consume_count=%lld\n", atomic64_read(&priv->rx_consume_count));
	seq_printf(m, "rx_refill_count=%lld\n", atomic64_read(&priv->rx_refill_count));
	seq_printf(m, "rx_packets=%lld\n", atomic64_read(&priv->rx_packets));
	seq_printf(m, "rx_bytes=%lld\n", atomic64_read(&priv->rx_bytes));
	seq_printf(m, "rx_dropped=%lld\n", atomic64_read(&priv->rx_dropped));
	seq_printf(m, "rx_dma_map_ok=%lld\n", atomic64_read(&priv->rx_dma_map_ok));
	seq_printf(m, "rx_dma_map_fail=%lld\n", atomic64_read(&priv->rx_dma_map_fail));
	seq_printf(m, "rx_dma_unmap=%lld\n", atomic64_read(&priv->rx_dma_unmap));
	seq_printf(m, "rx_truncated=%lld\n", atomic64_read(&priv->rx_truncated));
	seq_printf(m, "rx_no_posted=%lld\n", atomic64_read(&priv->rx_no_posted));
	seq_printf(m, "irq_count=%lld\n", atomic64_read(&priv->irq_count));
	seq_printf(m, "irq_mask_count=%lld\n", atomic64_read(&priv->irq_mask_count));
	seq_printf(m, "irq_unmask_count=%lld\n", atomic64_read(&priv->irq_unmask_count));
	seq_printf(m, "napi_schedule_count=%lld\n", atomic64_read(&priv->napi_schedule_count));
	seq_printf(m, "napi_poll_count=%lld\n", atomic64_read(&priv->napi_poll_count));
	seq_printf(m, "napi_complete_count=%lld\n", atomic64_read(&priv->napi_complete_count));
	seq_printf(m, "napi_budget_exhaust_count=%lld\n", atomic64_read(&priv->napi_budget_exhaust_count));
	seq_printf(m, "napi_work_total=%lld\n", atomic64_read(&priv->napi_work_total));
	seq_printf(m, "ring_full_count=%lld\n", atomic64_read(&priv->ring_full_count));
	seq_printf(m, "ring_empty_count=%lld\n", atomic64_read(&priv->ring_empty_count));
	seq_printf(m, "device_notify_count=%lld\n", atomic64_read(&priv->device_notify_count));
	seq_printf(m, "device_tx_processed=%lld\n", atomic64_read(&priv->device_tx_processed));
	seq_printf(m, "device_rx_produced=%lld\n", atomic64_read(&priv->device_rx_produced));
	seq_printf(m, "last_tx_len=%lld\n", atomic64_read(&priv->last_tx_len));
	seq_printf(m, "last_tx_proto=%#llx\n", atomic64_read(&priv->last_tx_proto));
	seq_printf(m, "last_rx_len=%lld\n", atomic64_read(&priv->last_rx_len));
	seq_printf(m, "last_rx_proto=%#llx\n", atomic64_read(&priv->last_rx_proto));
	return 0;
}

static int stage07_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, stage07_stats_show, inode->i_private);
}

static const struct file_operations stage07_stats_fops = {
	.owner = THIS_MODULE,
	.open = stage07_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*========================== debugfs queue dump ==========================*/

/*
 * stage07_queues_show — debugfs queues 文件的 seq_file 读函数
 *
 * 【文件路径】
 * /sys/kernel/debug/netdev_stage07/queues
 *
 * 【输出内容】
 * - TX/RX 6 个 index 的当前值
 * - 每个 queue 前 16 个 slot 的状态（desc state / slot state / len / skb 指针）
 *
 * 【为什么只 dump 16 个 slot？】
 * ring_size 可能很大（128），全量 dump 输出太长。
 * STAGE07_MAX_QUEUE_DUMP=16 是合理的调试信息量。
 */
static int stage07_queues_show(struct seq_file *m, void *v)
{
	struct net_device *ndev = m->private;
	struct stage07_priv *priv = netdev_priv(ndev);
	u16 i;

	/* TX index 快照 */
	seq_printf(m,
		   "TX submit=%u notify=%u complete=%u inflight=%u done=%u\n",
		   priv->txq.submit_idx, priv->txq.notify_idx, priv->txq.complete_idx,
		   priv->tx_inflight, priv->tx_done);

	/* TX slot dump（最多 16 个） */
	for (i = 0; i < min_t(u16, priv->txq.size, STAGE07_MAX_QUEUE_DUMP); ++i) {
		seq_printf(m,
			   "  tx[%u] desc=%s slot=%s len=%u skb=%p\n",
			   i,
			   stage07_state_name(priv->txq.desc[i].state),
			   stage07_state_name(priv->txq.slots[i].state),
			   priv->txq.desc[i].data_len,
			   priv->txq.slots[i].skb);
	}

	/* RX index 快照 */
	seq_printf(m,
		   "RX post=%u device=%u consume=%u posted=%u ready=%u\n",
		   priv->rxq.post_idx, priv->rxq.device_idx, priv->rxq.consume_idx,
		   priv->rx_posted, priv->rx_ready);

	/* RX slot dump（最多 16 个） */
	for (i = 0; i < min_t(u16, priv->rxq.size, STAGE07_MAX_QUEUE_DUMP); ++i) {
		seq_printf(m,
			   "  rx[%u] desc=%s slot=%s len=%u skb=%p\n",
			   i,
			   stage07_state_name(priv->rxq.desc[i].state),
			   stage07_state_name(priv->rxq.slots[i].state),
			   priv->rxq.desc[i].data_len,
			   priv->rxq.slots[i].skb);
	}
	return 0;
}

static int stage07_queues_open(struct inode *inode, struct file *file)
{
	return single_open(file, stage07_queues_show, inode->i_private);
}

static const struct file_operations stage07_queues_fops = {
	.owner = THIS_MODULE,
	.open = stage07_queues_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * stage07_debugfs_init — 创建 debugfs 目录和文件
 *
 * 【debugfs 路径】
 * /sys/kernel/debug/netdev_stage07/stats
 * /sys/kernel/debug/netdev_stage07/queues
 *
 * 【调用时机】
 * register_netdev() 成功之后。
 *
 * 【清理】
 * 在模块卸载时通过 debugfs_remove_recursive() 清理。
 */
static void stage07_debugfs_init(struct stage07_priv *priv)
{
	priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
	if (IS_ERR_OR_NULL(priv->dbg_dir)) {
		priv->dbg_dir = NULL;
		return;
	}
	debugfs_create_file("stats", 0444, priv->dbg_dir, priv->ndev, &stage07_stats_fops);
	debugfs_create_file("queues", 0444, priv->dbg_dir, priv->ndev, &stage07_queues_fops);
}

/*========================== 资源清理 ==========================*/

/*
 * stage07_cleanup_rx_queue — 清理 RX queue（模块卸载时）
 *
 * 【调用时机】
 * rmmod netdev_stage07 时，在 stage07_exit() 中调用。
 *
 * 【操作】
 * 遍历所有 RX slot：
 * - 如果 slot 有 skb：DMA unmap + dev_kfree_skb_any
 * - 重置所有 slot 和 desc 为 FREE
 *
 * 【注意】
 * 这里不重置 index（因为 queue 即将被释放）。
 * 清理顺序：RX queue 先清理（因为 RX skb 由驱动分配），
 * TX queue 后清理（TX skb 来自协议栈，已经在 complete 时释放了）。
 */
static void stage07_cleanup_rx_queue(struct stage07_priv *priv)
{
	u16 i;

	for (i = 0; i < priv->rxq.size; ++i) {
		struct stage07_buf_slot *slot = &priv->rxq.slots[i];
		if (slot->skb) {
			dma_unmap_single(&priv->ndev->dev, slot->dma_addr,
					 slot->buf_len ?: priv->rx_buf_size,
					 DMA_FROM_DEVICE);
			dev_kfree_skb_any(slot->skb);
		}
		slot->skb = NULL;
		slot->dma_addr = 0;
		slot->buf_len = 0;
		slot->data_len = 0;
		slot->state = S07_SLOT_FREE;
		priv->rxq.desc[i].dma_addr = 0;
		priv->rxq.desc[i].data_len = 0;
		priv->rxq.desc[i].state = S07_SLOT_FREE;
	}
}

/*
 * stage07_cleanup_tx_queue — 清理 TX queue（模块卸载时）
 *
 * 【注意】
 * TX slot 的 skb 应该已经在 NAPI poll 中通过 dev_consume_skb_any() 释放了。
 * 但如果模块非正常卸载（rmmod -f），可能还有 in-flight skb，需要这里兜底清理。
 */
static void stage07_cleanup_tx_queue(struct stage07_priv *priv)
{
	u16 i;

	for (i = 0; i < priv->txq.size; ++i) {
		struct stage07_buf_slot *slot = &priv->txq.slots[i];
		if (slot->skb) {
			dma_unmap_single(&priv->ndev->dev, slot->dma_addr,
					 slot->data_len, DMA_TO_DEVICE);
			dev_kfree_skb_any(slot->skb);
		}
		slot->skb = NULL;
		slot->dma_addr = 0;
		slot->buf_len = 0;
		slot->data_len = 0;
		slot->state = S07_SLOT_FREE;
		priv->txq.desc[i].dma_addr = 0;
		priv->txq.desc[i].data_len = 0;
		priv->txq.desc[i].state = S07_SLOT_FREE;
	}
}

/*========================== 队列分配 ==========================*/

/*
 * stage07_alloc_queues — 分配 TX/RX queue 内存
 *
 * 【调用时机】
 * stage07_init() 中，register_netdev() 之前调用。
 *
 * 【分配内容】
 * - txq.desc: TX descriptor ring（struct stage07_desc * 数组）
 * - txq.slots: TX buffer slot ring（struct stage07_buf_slot * 数组）
 * - rxq.desc: RX descriptor ring
 * - rxq.slots: RX buffer slot ring
 *
 * 【内存来源】
 * kcalloc（GFP_KERNEL，可睡眠），适合初始化时分配。
 *
 * 【初始化流程】
 * 1. kcalloc 分配 4 个 ring
 * 2. 重置所有 index 和状态
 * 3. 对 RX queue：用 stage07_post_rx_slot 预先填充所有 slot
 *
 * 【返回值】
 * 0: 成功
 * -ENOMEM: 内存分配失败
 */
static int stage07_alloc_queues(struct stage07_priv *priv)
{
	size_t qsz = sizeof(struct stage07_desc) * priv->txq.size;
	size_t ssz = sizeof(struct stage07_buf_slot) * priv->txq.size;
	u16 i;
	int ret;

	priv->txq.desc = kcalloc(priv->txq.size, sizeof(*priv->txq.desc), GFP_KERNEL);
	priv->txq.slots = kcalloc(priv->txq.size, sizeof(*priv->txq.slots), GFP_KERNEL);
	priv->rxq.desc = kcalloc(priv->rxq.size, sizeof(*priv->rxq.desc), GFP_KERNEL);
	priv->rxq.slots = kcalloc(priv->rxq.size, sizeof(*priv->rxq.slots), GFP_KERNEL);
	if (!priv->txq.desc || !priv->txq.slots || !priv->rxq.desc || !priv->rxq.slots)
		return -ENOMEM;

	/* 清零所有 ring */
	memset(priv->txq.desc, 0, qsz);
	memset(priv->txq.slots, 0, ssz);
	memset(priv->rxq.desc, 0, qsz);
	memset(priv->rxq.slots, 0, ssz);

	/* 给每个 slot 编号（用于 debug dump） */
	for (i = 0; i < priv->txq.size; ++i) {
		priv->txq.slots[i].id = i;
		priv->rxq.slots[i].id = i;
	}

	/* 重置 queue index 状态 */
	stage07_reset_queue_state(priv);

	/* 预先填充所有 RX slots（refill 的来源） */
	for (i = 0; i < priv->rxq.size; ++i) {
		ret = stage07_post_rx_slot(priv, i);
		if (ret)
			return ret;
	}
	return 0;
}

/*
 * stage07_free_queues — 释放 TX/RX queue 内存
 *
 * 【调用时机】
 * - stage07_init() 失败时的错误路径
 * - stage07_exit() 中
 *
 * 【清理顺序】
 * TX queue 先清理（TX skb 在 complete 时已释放，但可能有残留）
 * RX queue 后清理（RX skb 由驱动自己分配，需要手动释放）
 */
static void stage07_free_queues(struct stage07_priv *priv)
{
	stage07_cleanup_tx_queue(priv);
	stage07_cleanup_rx_queue(priv);
	kfree(priv->txq.desc);
	kfree(priv->txq.slots);
	kfree(priv->rxq.desc);
	kfree(priv->rxq.slots);
	priv->txq.desc = NULL;
	priv->txq.slots = NULL;
	priv->rxq.desc = NULL;
	priv->rxq.slots = NULL;
}

/*
 * stage07_setup — 初始化 net_device 的通用字段
 *
 * 【调用时机】
 * alloc_netdev 之后，register_netdev 之前调用。
 *
 * 【ether_setup 做了什么】
 * - 设置 dev->addr_len = ETH_ALEN（MAC 地址长度 = 6）
 * - 设置 dev->type = ARPHRD_ETHER（ARP 硬件类型）
 * - 设置 dev->tx_queue_len = DEFAULT_TX_QUEUE_LEN
 * - 初始化 dev->broadcast（广播 MAC 地址）
 * - 设置 dev->flags（IFF_BROADCAST / IFF_MULTICAST 等）
 */
static void stage07_setup(struct net_device *ndev)
{
	ether_setup(ndev);
	ndev->netdev_ops = &stage07_netdev_ops;
	ndev->flags |= IFF_NOARP; /* 不需要 ARP（我们是伪设备） */
	ndev->features |= NETIF_F_HIGHDMA; /* 支持 HIGHmem DMA */
	ndev->watchdog_timeo = 5 * HZ; /* TX watchdog 5 秒 */
	eth_hw_addr_random(ndev); /* 随机生成 MAC 地址 */
}

/*========================== 模块入口 ==========================*/

static int __init stage07_init(void)
{
	struct stage07_priv *priv;
	int ret;

	/* 参数合法性检查（防止用户传参错误） */
	if (ring_size < 8)
		ring_size = 8;
	if (napi_weight < 8)
		napi_weight = 8;
	if (rx_buf_size < 256)
		rx_buf_size = 256;

	/* 分配 net_device（ndev->ml_priv 将指向 stage07_priv） */
	stage07_dev = alloc_netdev(sizeof(struct stage07_priv), ifname,
				   NET_NAME_UNKNOWN, stage07_setup);
	if (!stage07_dev)
		return -ENOMEM;

	priv = netdev_priv(stage07_dev);
	priv->ndev = stage07_dev;
	priv->rx_buf_size = rx_buf_size;
	priv->txq.size = ring_size;
	priv->rxq.size = ring_size;
	spin_lock_init(&priv->state_lock);
	spin_lock_init(&priv->txq.lock);
	spin_lock_init(&priv->rxq.lock);

	/* 设置 DMA 能力（64bit，回退到 32bit） */
	ret = stage07_prepare_dma_caps(stage07_dev);
	if (ret)
		netdev_warn(stage07_dev, "dma_set_mask_and_coherent failed: %d\n", ret);

	/* 注册 NAPI（注意：napi_enable 在 open 时调用，不是在这里） */
	STAGE07_NETIF_NAPI_ADD(stage07_dev, &priv->napi, stage07_poll, napi_weight);

	/* 分配 TX/RX queue 内存并预填充 RX slots */
	ret = stage07_alloc_queues(priv);
	if (ret)
		goto err_napi;

	/* 注册 net_device（成功后 userspace 可 ifconfig 看到设备） */
	ret = register_netdev(stage07_dev);
	if (ret)
		goto err_queue;

	/* 创建 debugfs 文件 */
	stage07_debugfs_init(priv);
	pr_info("stage07 loaded: ifname=%s ring_size=%d napi_weight=%d rx_buf_size=%d\n",
		stage07_dev->name, ring_size, napi_weight, rx_buf_size);
	return 0;

err_queue:
	stage07_free_queues(priv);
err_napi:
	netif_napi_del(&priv->napi);
	free_netdev(stage07_dev);
	stage07_dev = NULL;
	return ret;
}

/*========================== 模块出口 ==========================*/

static void __exit stage07_exit(void)
{
	struct stage07_priv *priv;

	if (!stage07_dev)
		return;

	priv = netdev_priv(stage07_dev);
	unregister_netdev(stage07_dev);       /* 移除 net_device */
	debugfs_remove_recursive(priv->dbg_dir); /* 清理 debugfs */
	netif_napi_del(&priv->napi);          /* 移除 NAPI */
	stage07_free_queues(priv);             /* 释放 queue 内存 */
	free_netdev(stage07_dev);              /* 释放 net_device */
	stage07_dev = NULL;
	pr_info("stage07 unloaded\n");
}

module_init(stage07_init);
module_exit(stage07_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI + user project");
MODULE_DESCRIPTION("stage07 real queue model v1");
