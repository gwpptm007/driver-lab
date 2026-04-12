// SPDX-License-Identifier: GPL-2.0
/*
 * stage04x.c - stage04 ring / DMA / RX replenishment teaching driver
 *
 * 这个驱动不是要模拟一张“完整真实网卡”，而是把下面这些概念落成
 * 一个可以观察、可以调试、可以做 smoke 的教学型实现：
 *
 *   1. TX/RX descriptor ring
 *   2. ownership（CPU / device）
 *   3. streaming DMA map / unmap
 *   4. RX buffer 预投递
 *   5. NAPI poll drain done descriptors
 *   6. RX replenishment（处理完一个 slot 后立刻补 fresh buffer）
 *
 * 整体数据路径：
 *
 *   userspace sender
 *       -> ndo_start_xmit()
 *       -> map TX skb as DMA_TO_DEVICE
 *       -> 取一个 RX posted descriptor
 *       -> “教学型 device”把 TX payload 拷到 RX buffer
 *       -> 标记 RX desc DONE，raise irq -> napi_schedule
 *       -> poll drain RX done desc
 *       -> dma_unmap RX buffer / netif_receive_skb()
 *       -> refill 同一个 RX slot
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

#define DRV_NAME "stage04x"

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
#define STAGE04_U64_UPDATE_BEGIN(syncp, flags) \
	do { (flags) = u64_stats_update_begin_irqsave((syncp)); } while (0)
#define STAGE04_U64_FETCH_BEGIN(syncp, start) \
	do { (start) = u64_stats_fetch_begin((syncp)); } while (0)
#define STAGE04_U64_FETCH_RETRY(syncp, start) \
	u64_stats_fetch_retry((syncp), (start))
#else
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

struct stage04_tx_desc {
	dma_addr_t dma_addr;
	u32 data_len;
	u8 owner;
	u8 state;
};

struct stage04_rx_desc {
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	u32 buf_len;
	u32 data_len;
	u8 owner;
	u8 state;
};

struct stage04_priv {
	struct net_device *ndev;
	struct dentry *dbg_dir;
	struct napi_struct napi;
	struct packet_type rx_pkt_type;
	spinlock_t ring_lock;
	struct u64_stats_sync syncp;
	bool irq_masked;

	struct stage04_tx_desc *tx_ring;
	struct stage04_rx_desc *rx_ring;
	u16 ring_size;
	u16 tx_prod;
	u16 rx_hw_pos;
	u16 rx_poll_pos;
	u16 rx_posted;
	u16 rx_done;

	u32 rx_buf_size;

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

static int stage04_prepare_dma_caps(struct net_device *ndev)
{
	int ret;

	/*
	 * 教学型 net_device 默认没有真实硬件父设备，这里尽量把 DMA mask 打开。
	 * 如果宿主环境不支持，这个阶段会在实际 smoke 时暴露出来，后续再按测试结果收口。
	 */
	ndev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	ndev->dev.dma_mask = &ndev->dev.coherent_dma_mask;
	ret = dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(64));
	if (ret)
		ret = dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(32));
	return ret;
}

static int stage04_refill_rx_slot(struct stage04_priv *priv, u16 idx)
{
	struct stage04_rx_desc *rxd = &priv->rx_ring[idx];
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	unsigned long irq_flags;

	skb = netdev_alloc_skb_ip_align(priv->ndev, priv->rx_buf_size);
	if (!skb) {
		stage04_count_rx_refill(priv, false);
		return -ENOMEM;
	}

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
	dma_sync_single_for_device(&priv->ndev->dev, dma_addr, priv->rx_buf_size,
				   DMA_FROM_DEVICE);

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
 * stage04_rx_pkt_type_func - packet_type handler for ethertype 0x88B7
 *
 * 【为什么需要这个 handler？】
 *   netif_receive_skb() 收到一个 skb 后，会查找 ethertype 对应的
 *   packet_type 并调用其 func。如果 ethertype 没有注册，帧被 drop。
 *   0x88B7 是实验协议，没有内置 handler，所以需要我们自己注册一个。
 *
 * 【handler 的职责】
 *   → 返回 NET_RX_SUCCESS 表示"已处理，请不要 drop"
 *   → 不需要真的做任何事，因为 skb 已经在 poll 中送到了 netif_receive_skb
 *   → 这里只是为了让 ethertype 0x88B7 不被当作"未知协议"而 drop
 */
static int stage04_rx_pkt_type_func(struct sk_buff *skb, struct net_device *dev,
				     struct packet_type *ptype,
				     struct net_device *orig_dev)
{
	return NET_RX_SUCCESS;
}

static void stage04_raise_irq(struct stage04_priv *priv)
{
	unsigned long flags;
	bool do_schedule = false;

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

		spin_lock_irqsave(&priv->ring_lock, flags);
		idx = priv->rx_poll_pos;
		rxd = &priv->rx_ring[idx];
		if (rxd->state != STAGE04_DESC_DONE ||
		    rxd->owner != STAGE04_OWNER_CPU || !rxd->skb) {
			spin_unlock_irqrestore(&priv->ring_lock, flags);
			break;
		}

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

		dma_unmap_single(&priv->ndev->dev, dma_addr, buf_len, DMA_FROM_DEVICE);
		stage04_count_rx_dma_unmap(priv);
		skb_put(skb, len);
		proto = eth_type_trans(skb, priv->ndev);
		rc = netif_receive_skb(skb);
		stage04_count_rx_receive(priv, len, proto, rc);
		if (stage04_refill_rx_slot(priv, idx))
			netdev_warn(priv->ndev, "refill rx slot %u failed\n", idx);
		work_done++;
	}

	if (work_done == budget)
		exhausted = true;
	stage04_count_napi_poll(priv, budget, work_done, exhausted);

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
			priv->irq_masked = false;
		}
		spin_unlock_irqrestore(&priv->ring_lock, flags);

		if (!has_more) {
			stage04_count_napi_complete(priv);
			stage04_count_irq_unmasked(priv);
		}
	}

	return work_done;
}

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

	if (unlikely(skb_is_nonlinear(skb))) {
		if (skb_linearize(skb)) {
			stage04_count_tx_drop(priv);
			dev_kfree_skb_any(skb);
			return NETDEV_TX_OK;
		}
		stage04_count_tx_linearize(priv);
	}

	stage04_count_tx_success(priv, skb->len, skb->protocol);

	tx_dma = dma_map_single(&ndev->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(&ndev->dev, tx_dma)) {
		stage04_count_tx_dma_map(priv, false);
		stage04_count_tx_drop(priv);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	stage04_count_tx_dma_map(priv, true);
	dma_sync_single_for_device(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);

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

	txd->dma_addr = tx_dma;
	txd->data_len = skb->len;
	txd->owner = STAGE04_OWNER_DEV;
	txd->state = STAGE04_DESC_BUSY;

	if (stage04_find_posted_rx_slot(priv, &rx_idx)) {
		txd->owner = STAGE04_OWNER_CPU;
		txd->state = STAGE04_DESC_EMPTY;
		spin_unlock_irqrestore(&priv->ring_lock, flags);
		dma_unmap_single(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);
		stage04_count_tx_dma_unmap(priv);
		stage04_count_rx_no_desc(priv);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	rxd = &priv->rx_ring[rx_idx];
	copy_len = min_t(u32, skb->len, rxd->buf_len);
	if (copy_len < skb->len)
		stage04_count_rx_truncated(priv);

	dma_sync_single_for_device(&ndev->dev, rxd->dma_addr, rxd->buf_len,
				   DMA_FROM_DEVICE);
	memcpy(rxd->skb->data, skb->data, copy_len);
	dma_sync_single_for_cpu(&ndev->dev, rxd->dma_addr, copy_len,
			    DMA_FROM_DEVICE);

	rxd->data_len = copy_len;
	rxd->owner = STAGE04_OWNER_CPU;
	rxd->state = STAGE04_DESC_DONE;
	if (priv->rx_posted > 0)
		priv->rx_posted--;
	priv->rx_done++;
	priv->rx_hw_pos = (rx_idx + 1) % priv->ring_size;
	stage04_count_rx_done(priv, priv->rx_done);

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

static int __init stage04_init(void)
{
	struct net_device *ndev;
	struct stage04_priv *priv;
	int ret;

	if (ring_size < 4)
		ring_size = 4;
	if (ring_size > 1024)
		ring_size = 1024;
	if (rx_buf_size < 256)
		rx_buf_size = 256;
	if (napi_weight < 1)
		napi_weight = 1;

	ndev = alloc_etherdev_mqs(sizeof(*priv), 1, 1);
	if (!ndev)
		return -ENOMEM;

	snprintf(ndev->name, IFNAMSIZ, "%s", ifname);
	eth_hw_addr_random(ndev);
	ndev->netdev_ops = &stage04_netdev_ops;
	ndev->mtu = 1500;
	ndev->min_mtu = 68;
	ndev->max_mtu = 1500;
	ndev->flags |= IFF_NOARP;
	ndev->features |= NETIF_F_HW_CSUM;

	ret = stage04_prepare_dma_caps(ndev);
	if (ret)
		netdev_warn(ndev, "dma_set_mask_and_coherent failed: %d\n", ret);

	priv = netdev_priv(ndev);
	memset(priv, 0, sizeof(*priv));
	priv->ndev = ndev;
	priv->ring_size = ring_size;
	priv->rx_buf_size = rx_buf_size;
	spin_lock_init(&priv->ring_lock);
	u64_stats_init(&priv->syncp);
	STAGE04_NETIF_NAPI_ADD(ndev, &priv->napi, stage04_poll, napi_weight);

	ret = stage04_init_rings(priv);
	if (ret)
		goto err_napi;

	ret = register_netdev(ndev);
	if (ret)
		goto err_rings;

	priv->rx_pkt_type.type = htons(0x88B7);
	priv->rx_pkt_type.dev = ndev;
	priv->rx_pkt_type.func = stage04_rx_pkt_type_func;
	dev_add_pack(&priv->rx_pkt_type);

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

static void __exit stage04_exit(void)
{
	struct stage04_priv *priv;

	if (!stage04_dev)
		return;

	priv = netdev_priv(stage04_dev);
	dev_remove_pack(&priv->rx_pkt_type);
	stage04_debugfs_exit(priv);
	unregister_netdev(stage04_dev);
	napi_disable(&priv->napi);
	netif_napi_del(&priv->napi);
	stage04_cleanup_rings(priv);
	free_netdev(stage04_dev);
	stage04_dev = NULL;
	pr_info("[%s] unloaded\n", DRV_NAME);
}

module_init(stage04_init);
module_exit(stage04_exit);

MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("stage04 ring/dma/replenishment teaching netdev");
MODULE_LICENSE("GPL");
