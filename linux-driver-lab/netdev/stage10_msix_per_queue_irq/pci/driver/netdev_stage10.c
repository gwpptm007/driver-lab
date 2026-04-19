// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage10.c — stage10: MSI-X per-queue interrupt
 *
 * 基于 stage09 多队列架构，引入真实 PCI device + MSI-X 中断：
 *
 * 1. PCI driver 框架（probe/remove/pci_driver）
 * 2. pci_alloc_irq_vectors() 分配 per-queue MSI-X vectors
 * 3. request_threaded_irq() 绑定每个 queue 的 NAPI
 * 4. BAR register 作为 doorbell（替代 stage09 的 soft raise_irq）
 * 5. /proc/interrupts 中可观测 per-queue IRQ 编号
 * 6. IRQ affinity 可通过 /sys/irq/<N>/smp_affinity 调节
 *
 * 上下文：Linux kernel netdev子系统 + PCI子系统
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/smp.h>
#include <linux/interrupt.h>

#include "../include/netdev_stage10_compat.h"

#define DRV_NAME    "netdev_stage10"
#define DRV_VERSION "1.0"

#define STAGE10_MAX_QUEUES      4
#define STAGE10_DEFAULT_NUM_QUEUES  2
#define STAGE10_DEFAULT_RING_SIZE  128
#define STAGE10_DEFAULT_NAPI_WEIGHT 64
#define STAGE10_DEFAULT_BACKEND_BATCH 64
#define STAGE10_DEFAULT_BACKEND_DELAY_US 0

#define STAGE10_TEST_PROTO  0x88B9
#define STAGE09_TEST_MAGIC  "STAGE09"

#define PCI_VENDOR_ID_STAGE10  0x1D9B
#define PCI_DEVICE_ID_STAGE10  0x1010

/* stage10 模块参数 */
static int num_queues = STAGE10_DEFAULT_NUM_QUEUES;
module_param(num_queues, int, 0644);
MODULE_PARM_DESC(num_queues, "Number of TX/RX queue pairs (default: 2, max: 4)");

static int ring_size = STAGE10_DEFAULT_RING_SIZE;
module_param(ring_size, int, 0644);
MODULE_PARM_DESC(ring_size, "Descriptor ring size (default: 128)");

static int napi_weight = STAGE10_DEFAULT_NAPI_WEIGHT;
module_param(napi_weight, int, 0644);
MODULE_PARM_DESC(napi_weight, "NAPI poll budget per queue (default: 64)");

static int backend_batch = STAGE10_DEFAULT_BACKEND_BATCH;
module_param(backend_batch, int, 0644);
MODULE_PARM_DESC(backend_batch, "Backend batch size (default: 64)");

static int backend_delay_us = STAGE10_DEFAULT_BACKEND_DELAY_US;
module_param(backend_delay_us, int, 0644);
MODULE_PARM_DESC(backend_delay_us, "Simulated backend delay in us (default: 0)");

/*========================================================
 *     底层数据结构（基本沿用 stage09）
 *========================================================*/

/* slot 状态机 */
enum stage10_slot_state {
    S10_SLOT_FREE = 0,
    S10_SLOT_USED = 1,
    S10_SLOT_DONE = 2,
};

/* 描述符（每端点一个 uint32_t） */
struct stage10_desc {
    uint32_t flags;
    uint32_t data_len;
    uint64_t addr;
} __attribute__((packed));

/* DMA buffer slot */
struct stage10_buf_slot {
    struct sk_buff *skb;
    dma_addr_t dma_addr;
    uint16_t buf_len;
    uint16_t data_len;
    enum stage10_slot_state state;
};

/* TX/RX ring */
struct stage10_ring {
    struct stage10_desc  *desc;
    struct stage10_buf_slot *slots;
    uint16_t size;

    /* TX indices */
    uint16_t submit_idx;    /* 软件提交 */
    uint16_t notify_idx;    /* backend 消费 */
    uint16_t complete_idx;  /* 软件回收 */

    /* RX indices */
    uint16_t post_idx;      /* 软件补充 */
    uint16_t device_idx;    /* backend 写入 */
    uint16_t consume_idx;   /* NAPI 消费 */

    uint16_t pad;
} __attribute__((aligned(64)));

/* Per-queue timeline（8 个时间戳 + 4 个 delta）*/
struct stage10_timeline {
    uint64_t last_submit_ns;
    uint64_t last_doorbell_ns;
    uint64_t last_backend_wakeup_ns;
    uint64_t last_backend_done_ns;
    uint64_t last_irq_ns;
    uint64_t last_poll_ns;
    uint64_t last_complete_ns;
    uint64_t last_consume_ns;

    uint64_t last_submit_to_doorbell_ns;
    uint64_t last_doorbell_to_backend_ns;
    uint64_t last_backend_to_irq_ns;
    uint64_t last_irq_to_poll_ns;
};

/* Per-queue 统计计数器 */
struct stage10_queue_stats {
    atomic64_t tx_submit_count;
    atomic64_t tx_complete_count;
    atomic64_t tx_packets;
    atomic64_t tx_bytes;
    atomic64_t tx_busy;
    atomic64_t tx_dropped;
    atomic64_t tx_dma_map_ok;
    atomic64_t tx_dma_map_fail;
    atomic64_t tx_dma_unmap;

    atomic64_t rx_post_count;
    atomic64_t rx_consume_count;
    atomic64_t rx_packets;
    atomic64_t rx_bytes;
    atomic64_t rx_dropped;
    atomic64_t rx_dma_map_ok;
    atomic64_t rx_dma_map_fail;
    atomic64_t rx_dma_unmap;

    atomic64_t doorbell_count;
    atomic64_t backend_schedule_count;
    atomic64_t backend_run_count;
    atomic64_t backend_tx_processed;
    atomic64_t backend_rx_produced;
    atomic64_t irq_count;
    atomic64_t napi_poll_count;
    atomic64_t napi_complete_count;
    atomic64_t napi_work_total;

    atomic64_t test_tx_submit_count;
    atomic64_t test_rx_consume_count;
};

/* Per-queue 上下文 */
struct stage10_queue {
    struct stage10_priv *priv;
    uint16_t qid;

    /* NAPI（每队列独立） */
    struct napi_struct napi;

    /* Backend work（每队列独立） */
    struct work_struct backend_work;

    /* MSI-X 相关 */
    unsigned int msix_vector;
    char irq_name[32];

    /* 握手标志 */
    bool doorbell_pending;
    bool backend_running;

    /* TX 状态 */
    uint16_t tx_inflight;
    uint16_t tx_done;

    /* RX 状态 */
    uint16_t rx_posted;
    uint16_t rx_ready;

    /* Per-queue ring */
    struct stage10_ring txq;
    struct stage10_ring rxq;

    /* Per-queue timeline */
    struct stage10_timeline timeline;

    /* Per-queue 统计 */
    struct stage10_queue_stats stats;
};

/* priv — 管理所有队列和全局资源 */
struct stage10_priv {
    struct net_device *ndev;
    struct pci_dev *pci_dev;
    spinlock_t state_lock;

    /* MSI-X */
    void __iomem *doorbell_bar;
    unsigned int num_vectors;

    /* Backend workqueue */
    struct workqueue_struct *backend_wq;

    /* debugfs */
    struct dentry *dbg_dir;

    /* 参数 */
    uint32_t num_queues;
    uint32_t ring_size;
    uint32_t napi_weight_val;
    uint32_t backend_batch_val;
    uint32_t rx_buf_size;

    atomic64_t rr_counter;
    atomic64_t open_count;
    atomic64_t stop_count;

    struct stage10_queue queues[STAGE10_MAX_QUEUES];
};

static struct pci_device_id stage10_pci_ids[] = {
    { PCI_VENDOR_ID_STAGE10, PCI_DEVICE_ID_STAGE10,
      PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, stage10_pci_ids);

/* 全局 ndev 指针（stage09 遗留，用于 stage10_exit） */
static struct net_device *stage10_ndev;

/*========================================================
 *     时间戳
 *========================================================*/
static inline uint64_t stage10_now_ns(void)
{
    struct timespec64 ts;
    ktime_get_ts64(&ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/*========================================================
 *     工具函数
 *========================================================*/
static inline uint16_t stage10_next_idx(uint16_t idx, uint16_t size)
{
    return (idx + 1) & (size - 1);
}

/*========================================================
 *     内存分配 / 释放
 *========================================================*/
static int stage10_alloc_ring(struct stage10_ring *r, uint16_t size, bool is_tx)
{
    r->desc = kcalloc(size, sizeof(struct stage10_desc), GFP_KERNEL);
    if (!r->desc)
        return -ENOMEM;

    r->slots = kcalloc(size, sizeof(struct stage10_buf_slot), GFP_KERNEL);
    if (!r->slots) {
        kfree(r->desc);
        r->desc = NULL;
        return -ENOMEM;
    }

    r->size = size;
    if (is_tx) {
        r->submit_idx = r->notify_idx = r->complete_idx = 0;
    } else {
        r->post_idx = r->device_idx = r->consume_idx = 0;
    }
    return 0;
}

static void stage10_free_ring(struct stage10_ring *r, bool is_rx)
{
    uint16_t i;

    if (is_rx) {
        for (i = 0; i < r->size; i++) {
            if (r->slots[i].skb) {
                dev_kfree_skb_any(r->slots[i].skb);
                r->slots[i].skb = NULL;
            }
        }
    }

    kfree(r->slots);
    r->slots = NULL;
    kfree(r->desc);
    r->desc = NULL;
}

/*========================================================
 *     PCI MSI-X 中断处理函数（per-queue）
 *========================================================*/
static irqreturn_t stage10_msix_handler(int irq, void *data)
{
    struct stage10_queue *q = data;

    q->timeline.last_irq_ns = stage10_now_ns();
    q->timeline.last_backend_to_irq_ns =
        q->timeline.last_irq_ns - q->timeline.last_backend_done_ns;
    atomic64_inc(&q->stats.irq_count);

    /* MSI-X 中断触发 NAPI — 真实硬件中断路径 */
    napi_schedule(&q->napi);

    return IRQ_HANDLED;
}

/*========================================================
 *     stage10_ring_doorbell — PCI BAR 写 doorbell（替代 soft raise_irq）
 *
 *     写 BAR offset = qid * 8，写任意值触发 MSI
 *========================================================*/
static void stage10_ring_doorbell(struct stage10_queue *q)
{
    struct stage10_priv *priv = q->priv;
    uint64_t doorbell_ns;

    if (!priv->doorbell_bar)
        return;

    q->doorbell_pending = true;
    doorbell_ns = stage10_now_ns();
    q->timeline.last_doorbell_ns = doorbell_ns;
    q->timeline.last_submit_to_doorbell_ns =
        doorbell_ns - q->timeline.last_submit_ns;
    atomic64_inc(&q->stats.doorbell_count);

    /* 写 BAR 触发 MSI（QEMU/pci_device 模型负责把这个写转换成 MSI 中断） */
    writel(1, priv->doorbell_bar + q->qid * 8);
}

/*========================================================
 *     RX buffer 补充（沿用 stage09）
 *========================================================*/
static int stage10_post_rx_one(struct stage10_queue *q)
{
    struct stage10_priv *priv = q->priv;
    struct net_device *ndev = priv->ndev;
    struct stage10_ring *r = &q->rxq;
    struct stage10_buf_slot *s;
    struct sk_buff *skb;
    dma_addr_t dma;
    uint16_t idx;

    if ((r->post_idx == r->device_idx) && (q->rx_posted == 0))
        return 0;

    idx = r->post_idx;
    s = &r->slots[idx];

    skb = netdev_alloc_skb_ip_align(ndev, priv->rx_buf_size);
    if (!skb)
        return -ENOMEM;

    dma = dma_map_single(ndev->dev.parent ? ndev->dev.parent : &ndev->dev,
                          skb->data, priv->rx_buf_size, DMA_FROM_DEVICE);
    if (dma_mapping_error(ndev->dev.parent ? ndev->dev.parent : &ndev->dev, dma)) {
        dev_kfree_skb_any(skb);
        return -ENOMEM;
    }

    s->skb = skb;
    s->dma_addr = dma;
    s->buf_len = priv->rx_buf_size;
    s->data_len = 0;
    s->state = S10_SLOT_USED;

    r->desc[idx].addr = (uint64_t)dma;
    r->desc[idx].data_len = priv->rx_buf_size;
    r->desc[idx].flags = 0x1; /* RX ready */

    r->post_idx = stage10_next_idx(r->post_idx, r->size);
    q->rx_posted++;
    atomic64_inc(&q->stats.rx_post_count);
    return 0;
}

/*========================================================
 *     stage10_backend_workfn — async backend 处理
 *
 *     上下文：workqueue 线程（WQ_UNBOUND）
 *     持有锁：priv->state_lock
 *
 *     处理：TX (NOTIFY→DONE) + RX (DEVICE→POSTED)
 *     然后 raise doorbell BAR → MSI → NAPI
 *========================================================*/
static void stage10_backend_workfn(struct work_struct *work)
{
    struct stage10_queue *q = container_of(work, struct stage10_queue, backend_work);
    struct stage10_priv *priv = q->priv;
    unsigned long flags;
    int processed = 0;
    int budget = priv->backend_batch_val;

    q->timeline.last_backend_wakeup_ns = stage10_now_ns();
    atomic64_inc(&q->stats.backend_run_count);

    spin_lock_irqsave(&priv->state_lock, flags);

    /* TX: notify → done */
    while (processed < budget && q->tx_inflight > 0) {
        struct stage10_ring *r = &q->txq;
        struct stage10_buf_slot *s;

        if (r->notify_idx == r->complete_idx)
            break;

        s = &r->slots[r->complete_idx];
        if (s->state != S10_SLOT_DONE)
            break;

        /* TX 完成 */
        if (s->skb) {
            dev_consume_skb_any(s->skb);
            s->skb = NULL;
        }
        memset(s, 0, sizeof(*s));
        r->complete_idx = stage10_next_idx(r->complete_idx, r->size);
        q->tx_inflight--;
        q->tx_done++;
        processed++;
        atomic64_inc(&q->stats.tx_complete_count);
    }

    /* RX: device(backend produce) → posted(NAPI consume) */
    while (processed < budget && q->rx_posted > 0) {
        struct stage10_ring *r = &q->rxq;
        struct stage10_buf_slot *s;
        uint16_t idx;

        if (r->device_idx == r->consume_idx)
            break;

        idx = r->device_idx;
        s = &r->slots[idx];
        if (s->state != S10_SLOT_USED)
            break;

        /* 模拟 backend 收到数据（字节复制 + 设置 data_len） */
        s->data_len = 64; /* 模拟一个最小帧 */
        r->desc[idx].data_len = s->data_len;
        r->desc[idx].flags = 0x2; /* DONE */
        s->state = S10_SLOT_DONE;

        r->device_idx = stage10_next_idx(r->device_idx, r->size);
        q->rx_posted--;
        q->rx_ready++;
        processed++;
        atomic64_inc(&q->stats.backend_rx_produced);
    }

    q->timeline.last_backend_done_ns = stage10_now_ns();
    q->backend_running = false;

    if (q->doorbell_pending) {
        q->doorbell_pending = false;
        spin_unlock_irqrestore(&priv->state_lock, flags);

        /* 触发 MSI → NAPI */
        stage10_ring_doorbell(q);
        return;
    }

    spin_unlock_irqrestore(&priv->state_lock, flags);
}

/*========================================================
 *     stage10_mark_doorbell — enqueue backend work
 *========================================================*/
static void stage10_mark_doorbell(struct stage10_queue *q)
{
    struct stage10_priv *priv = q->priv;

    if (q->doorbell_pending || q->backend_running)
        return;

    q->doorbell_pending = true;
    atomic64_inc(&q->stats.backend_schedule_count);
    queue_work(priv->backend_wq, &q->backend_work);
}

/*========================================================
 *     stage10_napi_poll — NAPI 回调（per-queue）
 *
 *     budget 消耗后调用 napi_complete_done()
 *========================================================*/
static int stage10_napi_poll(struct napi_struct *napi, int budget)
{
    struct stage10_queue *q = container_of(napi, struct stage10_queue, napi);
    struct stage10_priv *priv = q->priv;
    unsigned long flags;
    int work_done = 0;

    q->timeline.last_poll_ns = stage10_now_ns();
    atomic64_inc(&q->stats.napi_poll_count);

    spin_lock_irqsave(&priv->state_lock, flags);

    /* TX complete */
    while (work_done < budget) {
        struct stage10_ring *r = &q->txq;
        struct stage10_buf_slot *s;

        if (q->tx_done == 0)
            break;

        s = &r->slots[r->complete_idx];
        if (s->state != S10_SLOT_DONE)
            break;

        memset(s, 0, sizeof(*s));
        r->complete_idx = stage10_next_idx(r->complete_idx, r->size);
        q->tx_done--;
        work_done++;
        atomic64_inc(&q->stats.napi_complete_count);
        netif_tx_wake_queue(netdev_get_tx_queue(priv->ndev, q->qid));
    }

    /* RX consume */
    while (work_done < budget && q->rx_ready > 0) {
        struct stage10_ring *r = &q->rxq;
        struct stage10_buf_slot *saved = &r->slots[r->consume_idx];
        struct sk_buff *skb;

        if (saved->state != S10_SLOT_DONE)
            break;

        skb = saved->skb;
        dma_unmap_single(priv->ndev->dev.parent ? priv->ndev->dev.parent : &priv->ndev->dev,
                          saved->dma_addr, saved->buf_len, DMA_FROM_DEVICE);
        atomic64_inc(&q->stats.rx_dma_unmap);

        skb_put(skb, saved->data_len);
        skb->protocol = eth_type_trans(skb, priv->ndev);

        /* test_rx 统计 */
        if (skb->protocol == htons(STAGE10_TEST_PROTO))
            atomic64_inc(&q->stats.test_rx_consume_count);

        netif_receive_skb(skb);

        atomic64_inc(&q->stats.rx_consume_count);
        atomic64_inc(&q->stats.rx_packets);
        atomic64_add(saved->data_len, &q->stats.rx_bytes);

        memset(saved, 0, sizeof(*saved));
        r->consume_idx = stage10_next_idx(r->consume_idx, r->size);
        if (q->rx_ready > 0) q->rx_ready--;
        work_done++;
        atomic64_inc(&q->stats.napi_work_total);

        /* 补充 RX buffer */
        if (stage10_post_rx_one(q))
            atomic64_inc(&q->stats.rx_dropped);
    }

    q->timeline.last_consume_ns = stage10_now_ns();

    if (work_done < budget) {
        napi_complete_done(napi, work_done);
        q->timeline.last_complete_ns = stage10_now_ns();
        spin_unlock_irqrestore(&priv->state_lock, flags);
        return work_done;
    }

    spin_unlock_irqrestore(&priv->state_lock, flags);
    return budget;
}

/*========================================================
 *     ndo_start_xmit — TX 入口
 *========================================================*/
static netdev_tx_t stage10_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    struct stage10_priv *priv = netdev_priv(ndev);
    struct stage10_queue *q;
    struct stage10_ring *r;
    struct stage10_buf_slot *s;
    uint16_t idx;
    uint16_t qid;
    unsigned long flags;
    bool is_test = false;

    /* 提取 qid（hash 或 round-robin） */
    if (skb->protocol == htons(STAGE10_TEST_PROTO) &&
        skb->data_len >= ETH_HLEN + 7 &&
        memcmp(skb->data + ETH_HLEN, STAGE09_TEST_MAGIC, 7) == 0) {
        is_test = true;
    }

    if (is_test) {
        qid = 0; /* test 流量固定 q0 */
    } else if (skb->protocol == htons(ETH_P_IP) ||
               skb->protocol == htons(ETH_P_IPV6)) {
        u32 hash = skb_get_hash(skb);
        if (!hash)
            hash = 0;
        qid = hash % priv->num_queues;
    } else {
        qid = (uint16_t)(atomic64_inc_return(&priv->rr_counter) % priv->num_queues);
    }

    q = &priv->queues[qid];
    r = &q->txq;

    spin_lock_irqsave(&priv->state_lock, flags);

    /* ring 满检查 */
    if (((r->submit_idx + 1) & (r->size - 1)) == r->complete_idx) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        atomic64_inc(&q->stats.tx_busy);
        return NETDEV_TX_BUSY;
    }

    idx = r->submit_idx;
    s = &r->slots[idx];

    /* DMA map */
    dma_addr_t dma = dma_map_single(ndev->dev.parent ? ndev->dev.parent : &ndev->dev,
                                     skb->data, skb->len, DMA_TO_DEVICE);
    if (dma_mapping_error(ndev->dev.parent ? ndev->dev.parent : &ndev->dev, dma)) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        atomic64_inc(&q->stats.tx_dma_map_fail);
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }

    s->skb = skb;
    s->dma_addr = dma;
    s->buf_len = skb->len;
    s->data_len = skb->len;
    s->state = S10_SLOT_USED;

    r->desc[idx].addr = (uint64_t)dma;
    r->desc[idx].data_len = skb->len;
    r->desc[idx].flags = 0x1;

    r->submit_idx = stage10_next_idx(r->submit_idx, r->size);
    q->tx_inflight++;

    q->timeline.last_submit_ns = stage10_now_ns();

    if (is_test)
        atomic64_inc(&q->stats.test_tx_submit_count);
    atomic64_inc(&q->stats.tx_submit_count);
    atomic64_add(skb->len, &q->stats.tx_bytes);

    /* 通知 backend 有新包 */
    stage10_mark_doorbell(q);

    spin_unlock_irqrestore(&priv->state_lock, flags);
    return NETDEV_TX_OK;
}

/*========================================================
 *     ndo_select_queue（沿用 stage09 hash 分发）
 *========================================================*/
static u16 stage10_select_queue(struct net_device *ndev, struct sk_buff *skb,
                                 struct net_device *sb_dev)
{
    struct stage10_priv *priv = netdev_priv(ndev);
    u32 hash;

    hash = skb_get_hash(skb);
    if (hash)
        return hash % priv->num_queues;
    return (u16)(atomic64_inc_return(&priv->rr_counter) % priv->num_queues);
}

/*========================================================
 *     ndo_open / ndo_stop
 *========================================================*/
static int stage10_open(struct net_device *ndev)
{
    struct stage10_priv *priv = netdev_priv(ndev);
    int i;

    atomic64_inc(&priv->open_count);

    for (i = 0; i < priv->num_queues; i++) {
        struct stage10_queue *q = &priv->queues[i];
        napi_enable(&q->napi);
    }

    netif_tx_start_all_queues(ndev);
    return 0;
}

static int stage10_stop(struct net_device *ndev)
{
    struct stage10_priv *priv = netdev_priv(ndev);
    int i;

    atomic64_inc(&priv->stop_count);
    netif_tx_stop_all_queues(ndev);

    for (i = 0; i < priv->num_queues; i++) {
        napi_disable(&priv->queues[i].napi);
    }
    return 0;
}

/*========================================================
 *     netdev_ops
 *========================================================*/
static const struct net_device_ops stage10_netdev_ops = {
    .ndo_start_xmit    = stage10_start_xmit,
    .ndo_select_queue  = stage10_select_queue,
    .ndo_open          = stage10_open,
    .ndo_stop          = stage10_stop,
};

/*========================================================
 *     debugfs: stats / queues / timeline / irqs
 *========================================================*/
static int stage10_stats_show(struct seq_file *m, void *v)
{
    struct stage10_priv *priv = dev_get_drvdata(m->private);
    struct net_device *ndev = priv->ndev;
    struct stage10_queue *q;
    int i;

    seq_printf(m, "ifname=%s num_queues=%d ring_size=%d napi_weight=%d backend_batch=%d open_count=%lld stop_count=%lld\n",
               ndev->name, priv->num_queues, priv->ring_size,
               priv->napi_weight_val, priv->backend_batch_val,
               (u64)atomic64_read(&priv->open_count),
               (u64)atomic64_read(&priv->stop_count));

    for (i = 0; i < priv->num_queues; i++) {
        q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx_submit=%lld tx_complete=%lld tx_packets=%lld tx_bytes=%lld tx_busy=%lld tx_drop=%lld "
                   "rx_post=%lld rx_consume=%lld rx_packets=%lld rx_bytes=%lld rx_drop=%lld "
                   "doorbell=%lld backend_schedule=%lld backend_run=%lld backend_tx=%lld backend_rx=%lld "
                   "irq=%lld napi_poll=%lld napi_complete=%lld napi_work=%lld "
                   "test_tx=%lld test_rx=%lld\n",
                   i,
                   (u64)atomic64_read(&q->stats.tx_submit_count),
                   (u64)atomic64_read(&q->stats.tx_complete_count),
                   (u64)atomic64_read(&q->stats.tx_packets),
                   (u64)atomic64_read(&q->stats.tx_bytes),
                   (u64)atomic64_read(&q->stats.tx_busy),
                   (u64)atomic64_read(&q->stats.tx_dropped),
                   (u64)atomic64_read(&q->stats.rx_post_count),
                   (u64)atomic64_read(&q->stats.rx_consume_count),
                   (u64)atomic64_read(&q->stats.rx_packets),
                   (u64)atomic64_read(&q->stats.rx_bytes),
                   (u64)atomic64_read(&q->stats.rx_dropped),
                   (u64)atomic64_read(&q->stats.doorbell_count),
                   (u64)atomic64_read(&q->stats.backend_schedule_count),
                   (u64)atomic64_read(&q->stats.backend_run_count),
                   (u64)atomic64_read(&q->stats.backend_tx_processed),
                   (u64)atomic64_read(&q->stats.backend_rx_produced),
                   (u64)atomic64_read(&q->stats.irq_count),
                   (u64)atomic64_read(&q->stats.napi_poll_count),
                   (u64)atomic64_read(&q->stats.napi_complete_count),
                   (u64)atomic64_read(&q->stats.napi_work_total),
                   (u64)atomic64_read(&q->stats.test_tx_submit_count),
                   (u64)atomic64_read(&q->stats.test_rx_consume_count));
    }
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(stage10_stats);

static int stage10_queues_show(struct seq_file *m, void *v)
{
    struct stage10_priv *priv = dev_get_drvdata(m->private);
    struct stage10_queue *q;
    int i;

    for (i = 0; i < priv->num_queues; i++) {
        q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx(submit=%u notify=%u complete=%u inflight=%u done=%u) "
                   "rx(post=%u device=%u consume=%u posted=%u ready=%u) "
                   "flags(doorbell=%d backend=%d irq_vector=%u)\n",
                   i,
                   q->txq.submit_idx, q->txq.notify_idx, q->txq.complete_idx,
                   q->tx_inflight, q->tx_done,
                   q->rxq.post_idx, q->rxq.device_idx, q->rxq.consume_idx,
                   q->rx_posted, q->rx_ready,
                   q->doorbell_pending, q->backend_running,
                   q->msix_vector);
    }
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(stage10_queues);

static int stage10_timeline_show(struct seq_file *m, void *v)
{
    struct stage10_priv *priv = dev_get_drvdata(m->private);
    struct stage10_queue *q;
    int i;

    for (i = 0; i < priv->num_queues; i++) {
        q = &priv->queues[i];
        seq_printf(m,
                   "q%u: submit_ns=%llu doorbell_ns=%llu backend_wakeup_ns=%llu backend_done_ns=%llu "
                   "irq_ns=%llu poll_ns=%llu complete_ns=%llu consume_ns=%llu "
                   "submit_to_doorbell_ns=%llu doorbell_to_backend_ns=%llu "
                   "backend_to_irq_ns=%llu irq_to_poll_ns=%llu\n",
                   i,
                   (unsigned long long)q->timeline.last_submit_ns,
                   (unsigned long long)q->timeline.last_doorbell_ns,
                   (unsigned long long)q->timeline.last_backend_wakeup_ns,
                   (unsigned long long)q->timeline.last_backend_done_ns,
                   (unsigned long long)q->timeline.last_irq_ns,
                   (unsigned long long)q->timeline.last_poll_ns,
                   (unsigned long long)q->timeline.last_complete_ns,
                   (unsigned long long)q->timeline.last_consume_ns,
                   (unsigned long long)q->timeline.last_submit_to_doorbell_ns,
                   (unsigned long long)q->timeline.last_doorbell_to_backend_ns,
                   (unsigned long long)q->timeline.last_backend_to_irq_ns,
                   (unsigned long long)q->timeline.last_irq_to_poll_ns);
    }
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(stage10_timeline);

static int stage10_irqs_show(struct seq_file *m, void *v)
{
    struct stage10_priv *priv = dev_get_drvdata(m->private);
    struct stage10_queue *q;
    int i;

    for (i = 0; i < priv->num_queues; i++) {
        q = &priv->queues[i];
        seq_printf(m,
                   "q%u: irq=%u vector=%u irq_count=%llu\n",
                   i,
                   pci_irq_vector(priv->pci_dev, i),
                   q->msix_vector,
                   (unsigned long long)atomic64_read(&q->stats.irq_count));
    }
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(stage10_irqs);

static void stage10_debugfs_init(struct stage10_priv *priv)
{
    struct dentry *dir;

    dir = debugfs_create_dir(DRV_NAME, NULL);
    priv->dbg_dir = dir;
    if (!dir)
        return;

    debugfs_create_file("stats", 0444, dir, &priv->ndev->dev, &stage10_stats_fops);
    debugfs_create_file("queues", 0444, dir, &priv->ndev->dev, &stage10_queues_fops);
    debugfs_create_file("timeline", 0444, dir, &priv->ndev->dev, &stage10_timeline_fops);
    debugfs_create_file("irqs", 0444, dir, &priv->ndev->dev, &stage10_irqs_fops);
}

static void stage10_debugfs_deinit(struct stage10_priv *priv)
{
    debugfs_remove_recursive(priv->dbg_dir);
    priv->dbg_dir = NULL;
}

/*========================================================
 *     PCI driver: probe
 *========================================================*/
static int stage10_pci_probe(struct pci_dev *pci_dev, const struct pci_device_id *id)
{
    struct net_device *ndev;
    struct stage10_priv *priv;
    int ret;
    int i;
    char irqname[32];

    pr_info(DRV_NAME ": probing PCI %04x:%04x\n", pci_dev->vendor, pci_dev->device);

    /* 启用 PCI device */
    ret = pci_enable_device(pci_dev);
    if (ret) {
        pr_err(DRV_NAME ": pci_enable_device failed: %d\n", ret);
        return ret;
    }

    pci_set_master(pci_dev);

    /* DMA mask */
    if (!dma_set_mask_and_coherent(&pci_dev->dev, DMA_BIT_MASK(64))) {
        pr_info(DRV_NAME ": using 64-bit DMA\n");
    } else if (!dma_set_mask_and_coherent(&pci_dev->dev, DMA_BIT_MASK(32))) {
        pr_info(DRV_NAME ": using 32-bit DMA\n");
    } else {
        pr_err(DRV_NAME ": no suitable DMA mask\n");
        ret = -ENODEV;
        goto err_pci_disable;
    }

    /* BAR 0: doorbell register（QEMU/pci_device 提供） */
    if (pci_resource_len(pci_dev, 0) == 0) {
        pr_warn(DRV_NAME ": BAR 0 not configured (no MSI trigger available)\n");
        pr_warn(DRV_NAME ": MSI-X will be allocated but doorbell falls back to soft-NAPI\n");
    }

    /* MSI-X: one vector per queue */
    priv = NULL;
    ret = pci_alloc_irq_vectors(pci_dev, num_queues, num_queues, PCI_IRQ_MSIX);
    if (ret < 0) {
        pr_err(DRV_NAME ": pci_alloc_irq_vectors (MSI-X) failed: %d\n", ret);
        goto err_pci_disable;
    }
    pr_info(DRV_NAME ": allocated %d MSI-X vectors\n", ret);

    /* 分配 netdev + priv */
    ndev = alloc_etherdev_mqs(sizeof(*priv), num_queues, num_queues);
    if (!ndev) {
        ret = -ENOMEM;
        goto err_free_vectors;
    }
    priv = netdev_priv(ndev);
    SET_NETDEV_DEV(ndev, &pci_dev->dev);
    priv->ndev = ndev;
    priv->pci_dev = pci_dev;
    stage10_ndev = ndev;
    dev_set_drvdata(&pci_dev->dev, priv);

    /* 参数 */
    priv->num_queues = min(num_queues, STAGE10_MAX_QUEUES);
    priv->ring_size = ring_size;
    priv->napi_weight_val = napi_weight;
    priv->backend_batch_val = backend_batch;
    priv->rx_buf_size = 2048;
    spin_lock_init(&priv->state_lock);

    /* BAR iomap（doorbell） */
    priv->doorbell_bar = pci_iomap(pci_dev, 0, 0);

    /* 创建 backend workqueue */
    priv->backend_wq = alloc_workqueue("stage10_backend", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);

    /* 初始化每个队列 */
    for (i = 0; i < priv->num_queues; i++) {
        struct stage10_queue *q = &priv->queues[i];

        q->priv = priv;
        q->qid = i;
        q->doorbell_pending = false;
        q->backend_running = false;
        q->tx_inflight = 0;
        q->tx_done = 0;
        q->rx_posted = 0;
        q->rx_ready = 0;

        memset(&q->stats, 0, sizeof(q->stats));
        memset(&q->timeline, 0, sizeof(q->timeline));

        /* MSI-X vector 请求 */
        snprintf(irqname, sizeof(irqname), "stage10-q%u", i);
        ret = request_irq(pci_irq_vector(pci_dev, i),
                           stage10_msix_handler,
                           IRQF_SHARED,
                           irqname,
                           q);
        if (ret) {
            pr_err(DRV_NAME ": request_irq failed for q%u: %d\n", i, ret);
            goto err_free_irqs;
        }
        q->msix_vector = pci_irq_vector(pci_dev, i);
        pr_info(DRV_NAME ": q%u → IRQ %u (vector %u)\n",
                i, pci_irq_vector(pci_dev, i), i);

        /* 分配 TX/RX ring */
        ret = stage10_alloc_ring(&q->txq, ring_size, true);
        if (ret) {
            pr_err(DRV_NAME ": TX ring alloc failed for q%u\n", i);
            goto err_free_irqs;
        }
        ret = stage10_alloc_ring(&q->rxq, ring_size, false);
        if (ret) {
            pr_err(DRV_NAME ": RX ring alloc failed for q%u\n", i);
            goto err_free_irqs;
        }

        /* RX buffer 预填充 */
        for (int j = 0; j < 64; j++) {
            if (stage10_post_rx_one(q) != 0)
                break;
        }

        /* NAPI 注册（每队列独立） */
        netif_napi_add_weight(ndev, &q->napi, stage10_napi_poll, napi_weight);
        INIT_WORK(&q->backend_work, stage10_backend_workfn);
    }

    /* netdev 基础设置 */
    ndev->netdev_ops = &stage10_netdev_ops;
    ndev->mtu = 1500;
    ether_setup(ndev);
    ndev->features |= NETIF_F_HW_CSUM | NETIF_F_SG;

    ret = register_netdev(ndev);
    if (ret) {
        pr_err(DRV_NAME ": register_netdev failed: %d\n", ret);
        goto err_free_napi;
    }

    /* debugfs */
    stage10_debugfs_init(priv);

    pr_info(DRV_NAME ": loaded ifname=%s num_queues=%d ring_size=%d napi_weight=%d backend_batch=%d\n",
            ndev->name, priv->num_queues, priv->ring_size,
            priv->napi_weight_val, priv->backend_batch_val);

    return 0;

err_free_napi:
    for (i = 0; i < priv->num_queues; i++) {
        netif_napi_del(&priv->queues[i].napi);
    }
err_free_irqs:
    for (i = 0; i < priv->num_queues; i++) {
        if (priv->queues[i].msix_vector)
            free_irq(pci_irq_vector(pci_dev, i), &priv->queues[i]);
    }
err_free_vectors:
    pci_free_irq_vectors(pci_dev);
err_pci_disable:
    pci_disable_device(pci_dev);
    return ret;
}

/*========================================================
 *     PCI driver: remove
 *========================================================*/
static void stage10_pci_remove(struct pci_dev *pci_dev)
{
    struct stage10_priv *priv = dev_get_drvdata(&pci_dev->dev);
    struct net_device *ndev;
    int i;

    if (!priv)
        return;

    ndev = priv->ndev;
    if (!ndev)
        return;

    stage10_debugfs_deinit(priv);
    unregister_netdev(ndev);

    /* 停止所有队列 */
    for (i = 0; i < priv->num_queues; i++) {
        struct stage10_queue *q = &priv->queues[i];
        cancel_work_sync(&q->backend_work);
        netif_napi_del(&q->napi);
        stage10_free_ring(&q->txq, false);
        stage10_free_ring(&q->rxq, true);
        free_irq(pci_irq_vector(pci_dev, i), q);
    }

    destroy_workqueue(priv->backend_wq);
    pci_free_irq_vectors(pci_dev);

    if (priv->doorbell_bar)
        pci_iounmap(pci_dev, priv->doorbell_bar);

    pci_disable_device(pci_dev);
    free_netdev(ndev);
    stage10_ndev = NULL;

    pr_info(DRV_NAME ": unloaded\n");
}

/*========================================================
 *     PCI driver struct
 *========================================================*/
static struct pci_driver stage10_pci_driver = {
    .name       = DRV_NAME,
    .id_table   = stage10_pci_ids,
    .probe      = stage10_pci_probe,
    .remove     = stage10_pci_remove,
};

module_pci_driver(stage10_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Lab");
MODULE_DESCRIPTION("stage10: MSI-X per-queue interrupt netdev driver");
MODULE_VERSION(DRV_VERSION);
