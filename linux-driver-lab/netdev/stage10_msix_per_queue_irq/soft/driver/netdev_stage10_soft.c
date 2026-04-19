// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage10_soft.c — stage10 纯软教学模型
 *
 * 基于 stage09 多队列架构，引入 MSI-X 语义模拟（无真实 PCI）：
 *
 * 1. 每队列对应一个 "vector"（struct stage10_vector）
 * 2. vector 有 target_cpu，支持 CPU 亲和性分发
 * 3. irq_work 模拟 MSI 中断处理：raise → irq_workfn → napi_schedule
 * 4. doorbell_pending 机制：backend_work 完成后触发 irq_work
 * 5. debugfs/vectors 观测 vector→queue→cpu 映射和处理统计
 *
 * 相比真实 PCI 版本（pci/）：
 *   - 无需 PCI 总线、pci_driver、MSI-X 硬件
 *   - 可在任何 Linux 环境运行和测试
 *   - 保持相同的上层语义（queue、NAPI、backend、timeline）
 *
 * 上下文：Linux kernel netdev子系统
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/jhash.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/spinlock.h>

#include "../include/netdev_stage10_compat.h"

#define DRV_NAME    "netdev_stage10_soft"
#define STAGE10_MAX_QUEUES      4
#define STAGE10_DEFAULT_NUM_QUEUES  2
#define STAGE10_DEFAULT_RING_SIZE  128
#define STAGE10_DEFAULT_NAPI_WEIGHT 64
#define STAGE10_DEFAULT_BACKEND_BATCH 64
#define STAGE10_DEFAULT_BACKEND_DELAY_US 0
#define STAGE10_QUEUE_DUMP_LIMIT 8

#define STAGE10_TEST_PROTO  0x88BA
#define STAGE10_TEST_MAGIC  "STAGE10"

/*========================================================
 *     底层数据结构
 *========================================================*/

/* slot 状态机 */
enum stage10_slot_state {
    S10_SLOT_FREE = 0,
    S10_SLOT_POSTED,
    S10_SLOT_SUBMITTED,
    S10_SLOT_DONE,
};

/* 描述符 — soft 版本不需要真实 DMA地址 */
struct stage10_desc {
    u32 data_len;
    u16 state;
    u16 flags;
};

/* Buffer slot — soft 版本用 bounce buffer 代替 DMA */
struct stage10_buf_slot {
    struct sk_buff *skb;
    void *buf;           /* bounce buffer for soft TX (no DMA) */
    u16 buf_len;
    u16 data_len;
    enum stage10_slot_state state;
    u16 id;
    u32 last_seq;
};

/* TX/RX ring */
struct stage10_ring {
    struct stage10_desc  *desc;
    struct stage10_buf_slot *slots;
    u16 size;

    /* TX indices */
    u16 submit_idx;
    u16 notify_idx;
    u16 complete_idx;

    /* RX indices */
    u16 post_idx;
    u16 device_idx;
    u16 consume_idx;
};

/* Per-queue timeline */
struct stage10_timeline {
    u64 last_submit_ns;
    u64 last_doorbell_ns;
    u64 last_backend_wakeup_ns;
    u64 last_backend_done_ns;
    u64 last_irq_ns;
    u64 last_poll_ns;
    u64 last_complete_ns;
    u64 last_consume_ns;
};

/*========================================================
 *     MSI-X 语义模拟：vector 结构
 *
 *     每个 queue 对应一个 "vector"，模拟真实 MSI-X 的：
 *     - vector_id：向量编号
 *     - target_cpu：向量处理的目标 CPU
 *     - raise/handle/schedule 计数器
 *========================================================*/
struct stage10_vector_stats {
    atomic64_t raise_count;
    atomic64_t handle_count;
    atomic64_t schedule_count;
};

struct stage10_vector {
    u16 vector_id;
    u16 qid;
    int target_cpu;
    char name[32];
    struct stage10_vector_stats stats;
    u64 last_raise_ns;
    u64 last_handle_ns;
    int last_raise_cpu;
    int last_handle_cpu;
};

/* Per-queue 统计 */
struct stage10_queue_stats {
    atomic64_t tx_submit_count;
    atomic64_t tx_complete_count;
    atomic64_t tx_packets;
    atomic64_t tx_bytes;
    atomic64_t tx_busy;
    atomic64_t tx_dropped;
    atomic64_t tx_linearize_count;
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
    u16 qid;

    /* NAPI */
    struct napi_struct napi;

    /* backend work */
    struct work_struct backend_work;

    /* irq work（模拟 MSI 中断处理） */
    struct work_struct irq_work;

    /* vector 关联 */
    u16 vector_id;
    bool irq_masked;
    bool doorbell_pending;
    bool backend_running;

    /* TX 状态 */
    u16 tx_inflight;
    u16 tx_done;

    /* RX 状态 */
    u16 rx_posted;
    u16 rx_ready;

    /* rings */
    struct stage10_ring txq;
    struct stage10_ring rxq;

    /* timeline */
    struct stage10_timeline timeline;

    /* 统计 */
    struct stage10_queue_stats stats;
};

/* priv */
struct stage10_priv {
    struct net_device *ndev;
    spinlock_t state_lock;

    /* backend workqueue */
    struct workqueue_struct *backend_wq;

    /* irq workqueue（用于 vector 中断模拟） */
    struct workqueue_struct *irq_wq;

    /* debugfs */
    struct dentry *dbg_dir;

    /* 参数 */
    u32 num_queues;
    u32 ring_size;
    u32 napi_weight;
    u32 rx_buf_size;
    u32 backend_delay_us;
    u32 backend_batch;

    atomic64_t rr_counter;
    atomic64_t open_count;
    atomic64_t stop_count;

    /* per-queue vectors */
    struct stage10_vector vectors[STAGE10_MAX_QUEUES];

    /* per-queue 上下文 */
    struct stage10_queue queues[STAGE10_MAX_QUEUES];
};

/*========================================================
 *     模块参数
 *========================================================*/
static char ifname[IFNAMSIZ] = "nds10s";
module_param_string(ifname, ifname, sizeof(ifname), 0444);
MODULE_PARM_DESC(ifname, "network interface name");

static unsigned int num_queues = STAGE10_DEFAULT_NUM_QUEUES;
module_param(num_queues, uint, 0444);
MODULE_PARM_DESC(num_queues, "number of TX/RX queue pairs");

static unsigned int ring_size = STAGE10_DEFAULT_RING_SIZE;
module_param(ring_size, uint, 0444);
MODULE_PARM_DESC(ring_size, "descriptor ring size");

static unsigned int napi_weight = STAGE10_DEFAULT_NAPI_WEIGHT;
module_param(napi_weight, uint, 0444);
MODULE_PARM_DESC(napi_weight, "NAPI poll budget");

static unsigned int backend_delay_us = STAGE10_DEFAULT_BACKEND_DELAY_US;
module_param(backend_delay_us, uint, 0644);
MODULE_PARM_DESC(backend_delay_us, "simulated backend delay in microseconds");

static unsigned int backend_batch = STAGE10_DEFAULT_BACKEND_BATCH;
module_param(backend_batch, uint, 0444);
MODULE_PARM_DESC(backend_batch, "backend batch size");

static unsigned int rx_buf_size = 2048;
module_param(rx_buf_size, uint, 0444);
MODULE_PARM_DESC(rx_buf_size, "RX buffer size");

/* 全局 ndev 指针 */
static struct net_device *stage10_soft_ndev;

/*========================================================
 *     时间戳
 *========================================================*/
static inline u64 stage10_now_ns(void)
{
    return ktime_get_ns();
}

/*========================================================
 *     工具函数
 *========================================================*/
static inline u16 stage10_next_idx(u16 idx, u16 size)
{
    return (idx + 1) % size;
}

/* 选择 vector 对应的 CPU（round-robin） */
static int stage10_pick_irq_cpu(u16 qid)
{
    int cpu, idx = 0;
    int want;

    if (num_online_cpus() == 0)
        return raw_smp_processor_id();

    want = qid % num_online_cpus();
    for_each_online_cpu(cpu) {
        if (idx == want)
            return cpu;
        idx++;
    }
    return raw_smp_processor_id();
}

static struct stage10_vector *stage10_get_vector(struct stage10_priv *priv, u16 qid)
{
    if (qid >= priv->num_queues)
        return NULL;
    return &priv->vectors[qid];
}

static inline bool stage10_is_test_frame(struct sk_buff *skb)
{
    if (ntohs(skb->protocol) != STAGE10_TEST_PROTO)
        return false;
    if (skb_headlen(skb) < ETH_HLEN + 8)
        return false;
    return memcmp(skb->data + ETH_HLEN, STAGE10_TEST_MAGIC, 7) == 0;
}

/*========================================================
 *     内存分配 / 释放
 *========================================================*/
static int stage10_alloc_ring(struct stage10_ring *r, u16 size)
{
    r->desc = kcalloc(size, sizeof(*r->desc), GFP_KERNEL);
    if (!r->desc)
        return -ENOMEM;
    r->slots = kcalloc(size, sizeof(*r->slots), GFP_KERNEL);
    if (!r->slots) {
        kfree(r->desc);
        r->desc = NULL;
        return -ENOMEM;
    }
    r->size = size;
    return 0;
}

static void stage10_free_ring(struct net_device *ndev, struct stage10_ring *r, bool is_rx)
{
    u16 i;
    if (!r->slots)
        goto out;
    for (i = 0; i < r->size; ++i) {
        struct stage10_buf_slot *s = &r->slots[i];
        if (s->skb) {
            dev_kfree_skb_any(s->skb);
            s->skb = NULL;
        }
        kfree(s->buf);
        s->buf = NULL;
    }
out:
    kfree(r->slots);
    kfree(r->desc);
    memset(r, 0, sizeof(*r));
}

static void stage10_reset_queue(struct stage10_queue *q)
{
    memset(q->txq.desc, 0, sizeof(*q->txq.desc) * q->txq.size);
    memset(q->txq.slots, 0, sizeof(*q->txq.slots) * q->txq.size);
    memset(q->rxq.desc, 0, sizeof(*q->rxq.desc) * q->rxq.size);
    memset(q->rxq.slots, 0, sizeof(*q->rxq.slots) * q->rxq.size);
    memset(&q->timeline, 0, sizeof(q->timeline));
    q->irq_masked = false;
    q->doorbell_pending = false;
    q->backend_running = false;
    q->tx_inflight = 0;
    q->tx_done = 0;
    q->rx_posted = 0;
    q->rx_ready = 0;
}

/*========================================================
 *     RX buffer 管理
 *========================================================*/
static int stage10_post_rx_one(struct stage10_queue *q)
{
    struct stage10_priv *priv = q->priv;
    struct net_device *ndev = priv->ndev;
    struct stage10_ring *r = &q->rxq;
    struct stage10_desc *d;
    struct stage10_buf_slot *s;
    struct sk_buff *skb;
    u16 idx;

    if (q->rx_posted >= r->size - 1)
        return -ENOSPC;
    idx = r->post_idx;
    d = &r->desc[idx];
    s = &r->slots[idx];
    if (s->state != S10_SLOT_FREE)
        return -EBUSY;

    skb = netdev_alloc_skb(ndev, rx_buf_size);
    if (!skb)
        return -ENOMEM;

    /* soft model: no DMA, just use skb directly */
    s->skb = skb;
    s->buf = NULL;
    s->buf_len = rx_buf_size;
    s->data_len = 0;
    s->state = S10_SLOT_POSTED;
    s->id = idx;

    d->data_len = rx_buf_size;
    d->state = S10_SLOT_POSTED;

    r->post_idx = stage10_next_idx(r->post_idx, r->size);
    q->rx_posted++;
    atomic64_inc(&q->stats.rx_post_count);
    return 0;
}

static void stage10_refill_rx_all(struct stage10_queue *q)
{
    while (q->rx_posted < q->rxq.size - 1) {
        if (stage10_post_rx_one(q))
            break;
    }
}

/*========================================================
 *     MSI-X 语义模拟：irq_workfn
 *
 *     模拟真实 MSI 中断处理：
 *     - 记录 irq timestamp
 *     - 递增 irq_count
 *     - 记录 vector handle 统计
 *     - 调用 napi_schedule() 触发 NAPI poll
 *========================================================*/
static void stage10_irq_workfn(struct work_struct *work)
{
    struct stage10_queue *q = container_of(work, struct stage10_queue, irq_work);
    struct stage10_priv *priv = q->priv;
    struct stage10_vector *vec = stage10_get_vector(priv, q->qid);
    unsigned long flags;

    spin_lock_irqsave(&priv->state_lock, flags);
    q->timeline.last_irq_ns = stage10_now_ns();
    atomic64_inc(&q->stats.irq_count);
    if (vec) {
        atomic64_inc(&vec->stats.handle_count);
        vec->last_handle_ns = q->timeline.last_irq_ns;
        vec->last_handle_cpu = raw_smp_processor_id();
    }

    /* 模拟 MSI 中断触发 NAPI */
    if (!q->irq_masked && napi_schedule_prep(&q->napi)) {
        q->irq_masked = true;
        __napi_schedule(&q->napi);
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);
}

/*========================================================
 *     MSI-X 语义模拟：raise_irq
 *
 *     触发 vector 中断（模拟 MSI 中断 raise）：
 *     - 记录 raise timestamp 和 CPU
 *     - 递增 raise_count / schedule_count
 *     - 发送到 vector 绑定的 target_cpu
 *========================================================*/
static void stage10_raise_irq(struct stage10_queue *q)
{
    struct stage10_priv *priv = q->priv;
    struct stage10_vector *vec = stage10_get_vector(priv, q->qid);

    if (vec) {
        atomic64_inc(&vec->stats.raise_count);
        atomic64_inc(&vec->stats.schedule_count);
        vec->last_raise_ns = stage10_now_ns();
        vec->last_raise_cpu = raw_smp_processor_id();
        if (cpu_online(vec->target_cpu))
            queue_work_on(vec->target_cpu, priv->irq_wq, &q->irq_work);
        else
            queue_work(priv->irq_wq, &q->irq_work);
        return;
    }
    queue_work(priv->irq_wq, &q->irq_work);
}

/*========================================================
 *     doorbell — 触发 backend work
 *========================================================*/
static void stage10_mark_doorbell(struct stage10_queue *q)
{
    q->timeline.last_doorbell_ns = stage10_now_ns();
    atomic64_inc(&q->stats.doorbell_count);
    atomic64_inc(&q->stats.backend_schedule_count);
    if (!q->doorbell_pending) {
        q->doorbell_pending = true;
        queue_work(q->priv->backend_wq, &q->backend_work);
    }
}

/*========================================================
 *     TX complete（在 NAPI poll 中）
 *========================================================*/
static void stage10_complete_tx_one(struct stage10_queue *q)
{
    struct stage10_ring *r = &q->txq;
    struct stage10_desc *d = &r->desc[r->complete_idx];
    struct stage10_buf_slot *s = &r->slots[r->complete_idx];

    if (!q->tx_done || s->state != S10_SLOT_DONE)
        return;

    /* soft model: no DMA unmap, just free resources */
    kfree(s->buf);
    s->buf = NULL;
    if (s->skb) {
        dev_consume_skb_any(s->skb);
        s->skb = NULL;
    }
    memset(s, 0, sizeof(*s));
    memset(d, 0, sizeof(*d));
    r->complete_idx = stage10_next_idx(r->complete_idx, r->size);
    q->tx_done--;
    q->tx_inflight--;
    q->timeline.last_complete_ns = stage10_now_ns();
    atomic64_inc(&q->stats.tx_complete_count);
}

/*========================================================
 *     RX consume（在 NAPI poll 中）
 *========================================================*/
static int stage10_consume_rx_one(struct stage10_queue *q)
{
    struct net_device *ndev = q->priv->ndev;
    struct stage10_ring *r = &q->rxq;
    struct stage10_desc *d = &r->desc[r->consume_idx];
    struct stage10_buf_slot *s = &r->slots[r->consume_idx];
    struct sk_buff *skb;
    u32 len;

    if (!q->rx_ready || s->state != S10_SLOT_DONE)
        return 0;

    skb = s->skb;
    len = s->data_len;

    /* soft model: no DMA unmap needed */
    skb_trim(skb, 0);
    skb_put(skb, len);
    skb->protocol = eth_type_trans(skb, ndev);
    if (stage10_is_test_frame(skb))
        atomic64_inc(&q->stats.test_rx_consume_count);
    netif_receive_skb(skb);

    memset(s, 0, sizeof(*s));
    memset(d, 0, sizeof(*d));
    r->consume_idx = stage10_next_idx(r->consume_idx, r->size);
    q->rx_ready--;
    q->timeline.last_consume_ns = stage10_now_ns();
    atomic64_inc(&q->stats.rx_consume_count);
    atomic64_inc(&q->stats.rx_packets);
    atomic64_add(len, &q->stats.rx_bytes);
    stage10_post_rx_one(q);
    return 1;
}

/*========================================================
 *     backend workfn — 异步处理 TX/RX
 *
 *     1. TX: notify → done（回收已完成的 TX slot）
 *     2. RX: posted → device（模拟 backend 收到数据）
 *     3. 调用 stage10_raise_irq() 触发 MSI 中断模拟
 *========================================================*/
static void stage10_backend_workfn(struct work_struct *work)
{
    struct stage10_queue *q = container_of(work, struct stage10_queue, backend_work);
    struct stage10_priv *priv = q->priv;
    unsigned long flags;
    int processed = 0;
    bool need_resched = false;

    if (priv->backend_delay_us)
        usleep_range(priv->backend_delay_us, priv->backend_delay_us + 50);

    spin_lock_irqsave(&priv->state_lock, flags);
    q->doorbell_pending = false;
    q->backend_running = true;
    q->timeline.last_backend_wakeup_ns = stage10_now_ns();
    atomic64_inc(&q->stats.backend_run_count);

    while (processed < priv->backend_batch) {
        struct stage10_ring *txr = &q->txq;
        struct stage10_ring *rxr = &q->rxq;
        struct stage10_desc *txd, *rxd;
        struct stage10_buf_slot *txs, *rxs;
        u16 txi, rxi;
        u32 copy_len;

        /* TX: notify_idx == submit_idx 表示没有新 TX */
        if (txr->notify_idx == txr->submit_idx)
            break;
        /* RX: 没有可用的 posted buffer */
        if (!q->rx_posted)
            break;

        txi = txr->notify_idx;
        rxi = rxr->device_idx;
        txd = &txr->desc[txi];
        txs = &txr->slots[txi];
        rxd = &rxr->desc[rxi];
        rxs = &rxr->slots[rxi];

        if (txs->state != S10_SLOT_SUBMITTED)
            break;
        if (rxs->state != S10_SLOT_POSTED)
            break;

        /* 模拟 backend 收到 TX 数据并转发到 RX */
        copy_len = min(txd->data_len, rxs->buf_len);
        skb_put(rxs->skb, copy_len);
        memcpy(rxs->skb->data, txs->buf, copy_len);
        rxs->data_len = copy_len;
        rxs->state = S10_SLOT_DONE;
        rxd->data_len = copy_len;
        rxd->state = S10_SLOT_DONE;
        rxr->device_idx = stage10_next_idx(rxr->device_idx, rxr->size);
        q->rx_posted--;
        q->rx_ready++;

        txs->state = S10_SLOT_DONE;
        txd->state = S10_SLOT_DONE;
        txr->notify_idx = stage10_next_idx(txr->notify_idx, txr->size);
        q->tx_done++;

        atomic64_inc(&q->stats.backend_tx_processed);
        atomic64_inc(&q->stats.backend_rx_produced);
        processed++;
    }

    q->timeline.last_backend_done_ns = stage10_now_ns();
    /* 触发 MSI-X 语义模拟（真实 PCI 版本这里会写 BAR） */
    if (processed)
        stage10_raise_irq(q);
    if (q->txq.notify_idx != q->txq.submit_idx)
        need_resched = true;
    q->backend_running = false;
    spin_unlock_irqrestore(&priv->state_lock, flags);

    if (need_resched)
        stage10_mark_doorbell(q);
}

/*========================================================
 *     NAPI poll — 消费 TX done 和 RX ready
 *========================================================*/
static int stage10_napi_poll(struct napi_struct *napi, int budget)
{
    struct stage10_queue *q = container_of(napi, struct stage10_queue, napi);
    struct stage10_priv *priv = q->priv;
    unsigned long flags;
    int work = 0;

    spin_lock_irqsave(&priv->state_lock, flags);
    q->timeline.last_poll_ns = stage10_now_ns();
    atomic64_inc(&q->stats.napi_poll_count);

    /* TX complete */
    while (q->tx_done)
        stage10_complete_tx_one(q);

    /* RX consume */
    while (q->rx_ready && work < budget)
        work += stage10_consume_rx_one(q);

    atomic64_add(work, &q->stats.napi_work_total);
    if (!q->rx_ready && !q->tx_done) {
        STAGE10_NAPI_COMPLETE(napi, work);
        q->irq_masked = false;
        atomic64_inc(&q->stats.napi_complete_count);
        if (q->doorbell_pending || q->txq.notify_idx != q->txq.submit_idx)
            stage10_mark_doorbell(q);
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);
    return work;
}

/*========================================================
 *     ndo_start_xmit
 *========================================================*/
static netdev_tx_t stage10_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    struct stage10_priv *priv = netdev_priv(ndev);
    struct stage10_queue *q;
    struct stage10_ring *r;
    struct stage10_desc *d;
    struct stage10_buf_slot *s;
    unsigned long flags;
    void *buf;
    u16 idx;
    u16 qid;

    qid = skb_get_queue_mapping(skb) % priv->num_queues;
    q = &priv->queues[qid];
    r = &q->txq;

    if (skb_is_nonlinear(skb) && skb_linearize(skb)) {
        atomic64_inc(&q->stats.tx_linearize_count);
        atomic64_inc(&q->stats.tx_dropped);
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }

    spin_lock_irqsave(&priv->state_lock, flags);
    if (q->tx_inflight >= r->size - 1) {
        atomic64_inc(&q->stats.tx_busy);
        spin_unlock_irqrestore(&priv->state_lock, flags);
        return NETDEV_TX_BUSY;
    }

    idx = r->submit_idx;
    d = &r->desc[idx];
    s = &r->slots[idx];
    if (s->state != S10_SLOT_FREE) {
        atomic64_inc(&q->stats.tx_busy);
        spin_unlock_irqrestore(&priv->state_lock, flags);
        return NETDEV_TX_BUSY;
    }

    /* soft model: use bounce buffer copy instead of DMA */
    buf = kmalloc(skb_headlen(skb), GFP_ATOMIC);
    if (!buf) {
        atomic64_inc(&q->stats.tx_dropped);
        spin_unlock_irqrestore(&priv->state_lock, flags);
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }
    memcpy(buf, skb->data, skb_headlen(skb));

    s->skb = skb;
    s->buf = buf;
    s->buf_len = skb_headlen(skb);
    s->data_len = skb_headlen(skb);
    s->state = S10_SLOT_SUBMITTED;
    s->id = idx;
    d->data_len = skb_headlen(skb);
    d->state = S10_SLOT_SUBMITTED;
    r->submit_idx = stage10_next_idx(r->submit_idx, r->size);
    q->tx_inflight++;

    q->timeline.last_submit_ns = stage10_now_ns();
    atomic64_inc(&q->stats.tx_submit_count);
    atomic64_inc(&q->stats.tx_packets);
    atomic64_add(skb_headlen(skb), &q->stats.tx_bytes);
    if (stage10_is_test_frame(skb))
        atomic64_inc(&q->stats.test_tx_submit_count);

    stage10_mark_doorbell(q);
    spin_unlock_irqrestore(&priv->state_lock, flags);
    return NETDEV_TX_OK;
}

/*========================================================
 *     ndo_select_queue
 *========================================================*/
static u16 stage10_select_queue(struct net_device *ndev, struct sk_buff *skb,
                                 struct net_device *sb_dev)
{
    struct stage10_priv *priv = netdev_priv(ndev);
    u32 hash = skb_get_hash(skb);

    if (hash)
        return reciprocal_scale(hash, priv->num_queues);
    return atomic64_inc_return(&priv->rr_counter) % priv->num_queues;
}

/*========================================================
 *     ndo_open / ndo_stop
 *========================================================*/
static int stage10_open(struct net_device *ndev)
{
    struct stage10_priv *priv = netdev_priv(ndev);
    unsigned long flags;
    int i;

    spin_lock_irqsave(&priv->state_lock, flags);
    for (i = 0; i < priv->num_queues; ++i) {
        stage10_reset_queue(&priv->queues[i]);
        stage10_refill_rx_all(&priv->queues[i]);
        napi_enable(&priv->queues[i].napi);
    }
    atomic64_inc(&priv->open_count);
    spin_unlock_irqrestore(&priv->state_lock, flags);

    netif_tx_start_all_queues(ndev);
    return 0;
}

static int stage10_stop(struct net_device *ndev)
{
    struct stage10_priv *priv = netdev_priv(ndev);
    int i;

    netif_tx_disable(ndev);
    if (priv->backend_wq)
        flush_workqueue(priv->backend_wq);
    for (i = 0; i < priv->num_queues; ++i)
        napi_disable(&priv->queues[i].napi);
    atomic64_inc(&priv->stop_count);
    return 0;
}

/*========================================================
 *     stats
 *========================================================*/
static void stage10_get_stats64(struct net_device *ndev, struct rtnl_link_stats64 *stats)
{
    struct stage10_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage10_queue *q = &priv->queues[i];
        stats->tx_packets += atomic64_read(&q->stats.tx_packets);
        stats->tx_bytes += atomic64_read(&q->stats.tx_bytes);
        stats->tx_dropped += atomic64_read(&q->stats.tx_dropped);
        stats->rx_packets += atomic64_read(&q->stats.rx_packets);
        stats->rx_bytes += atomic64_read(&q->stats.rx_bytes);
        stats->rx_dropped += atomic64_read(&q->stats.rx_dropped);
    }
}

static const struct net_device_ops stage10_netdev_ops = {
    .ndo_open = stage10_open,
    .ndo_stop = stage10_stop,
    .ndo_start_xmit = stage10_start_xmit,
    .ndo_select_queue = stage10_select_queue,
    .ndo_get_stats64 = stage10_get_stats64,
};

/*========================================================
 *     debugfs 实现
 *========================================================*/
static int stage10_stats_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage10_priv *priv = netdev_priv(ndev);
    int i;

    seq_printf(m, "ifname=%s num_queues=%u ring_size=%u napi_weight=%u backend_batch=%u open=%lld stop=%lld\n",
               ndev->name, priv->num_queues, priv->ring_size, priv->napi_weight,
               priv->backend_batch, atomic64_read(&priv->open_count),
               atomic64_read(&priv->stop_count));
    for (i = 0; i < priv->num_queues; ++i) {
        struct stage10_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx_submit=%lld tx_complete=%lld tx_packets=%lld tx_bytes=%lld tx_busy=%lld tx_drop=%lld "
                   "rx_post=%lld rx_consume=%lld rx_packets=%lld rx_bytes=%lld rx_drop=%lld "
                   "doorbell=%lld backend_run=%lld backend_tx=%lld backend_rx=%lld "
                   "irq=%lld napi_poll=%lld napi_complete=%lld napi_work=%lld test_tx=%lld test_rx=%lld\n",
                   q->qid,
                   atomic64_read(&q->stats.tx_submit_count),
                   atomic64_read(&q->stats.tx_complete_count),
                   atomic64_read(&q->stats.tx_packets),
                   atomic64_read(&q->stats.tx_bytes),
                   atomic64_read(&q->stats.tx_busy),
                   atomic64_read(&q->stats.tx_dropped),
                   atomic64_read(&q->stats.rx_post_count),
                   atomic64_read(&q->stats.rx_consume_count),
                   atomic64_read(&q->stats.rx_packets),
                   atomic64_read(&q->stats.rx_bytes),
                   atomic64_read(&q->stats.rx_dropped),
                   atomic64_read(&q->stats.doorbell_count),
                   atomic64_read(&q->stats.backend_run_count),
                   atomic64_read(&q->stats.backend_tx_processed),
                   atomic64_read(&q->stats.backend_rx_produced),
                   atomic64_read(&q->stats.irq_count),
                   atomic64_read(&q->stats.napi_poll_count),
                   atomic64_read(&q->stats.napi_complete_count),
                   atomic64_read(&q->stats.napi_work_total),
                   atomic64_read(&q->stats.test_tx_submit_count),
                   atomic64_read(&q->stats.test_rx_consume_count));
    }
    return 0;
}

static int stage10_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage10_stats_show, inode->i_private);
}

static const struct file_operations stage10_stats_fops = {
    .owner = THIS_MODULE,
    .open = stage10_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage10_queues_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage10_priv *priv = netdev_priv(ndev);
    int i, j;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage10_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx submit=%u notify=%u complete=%u inflight=%u done=%u | "
                   "rx post=%u device=%u consume=%u posted=%u ready=%u "
                   "irq_masked=%u doorbell_pending=%u backend_running=%u\n",
                   q->qid, q->txq.submit_idx, q->txq.notify_idx, q->txq.complete_idx,
                   q->tx_inflight, q->tx_done, q->rxq.post_idx, q->rxq.device_idx,
                   q->rxq.consume_idx, q->rx_posted, q->rx_ready,
                   q->irq_masked, q->doorbell_pending, q->backend_running);
        for (j = 0; j < min_t(u16, q->txq.size, STAGE10_QUEUE_DUMP_LIMIT); ++j)
            seq_printf(m, "  q%u txslot[%d]: state=%u len=%u\n", q->qid, j,
                       q->txq.slots[j].state, q->txq.slots[j].data_len);
        for (j = 0; j < min_t(u16, q->rxq.size, STAGE10_QUEUE_DUMP_LIMIT); ++j)
            seq_printf(m, "  q%u rxslot[%d]: state=%u len=%u\n", q->qid, j,
                       q->rxq.slots[j].state, q->rxq.slots[j].data_len);
    }
    return 0;
}

static int stage10_queues_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage10_queues_show, inode->i_private);
}

static const struct file_operations stage10_queues_fops = {
    .owner = THIS_MODULE,
    .open = stage10_queues_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage10_timeline_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage10_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage10_queue *q = &priv->queues[i];
        u64 d1 = q->timeline.last_doorbell_ns > q->timeline.last_submit_ns ?
                 q->timeline.last_doorbell_ns - q->timeline.last_submit_ns : 0;
        u64 d2 = q->timeline.last_backend_wakeup_ns > q->timeline.last_doorbell_ns ?
                 q->timeline.last_backend_wakeup_ns - q->timeline.last_doorbell_ns : 0;
        u64 d3 = q->timeline.last_irq_ns > q->timeline.last_backend_done_ns ?
                 q->timeline.last_irq_ns - q->timeline.last_backend_done_ns : 0;
        u64 d4 = q->timeline.last_poll_ns > q->timeline.last_irq_ns ?
                 q->timeline.last_poll_ns - q->timeline.last_irq_ns : 0;
        seq_printf(m,
                   "q%u: submit_ns=%llu doorbell_ns=%llu backend_wakeup_ns=%llu backend_done_ns=%llu "
                   "irq_ns=%llu poll_ns=%llu complete_ns=%llu consume_ns=%llu "
                   "submit_to_doorbell_ns=%llu doorbell_to_backend_ns=%llu "
                   "backend_to_irq_ns=%llu irq_to_poll_ns=%llu\n",
                   q->qid,
                   q->timeline.last_submit_ns, q->timeline.last_doorbell_ns,
                   q->timeline.last_backend_wakeup_ns, q->timeline.last_backend_done_ns,
                   q->timeline.last_irq_ns, q->timeline.last_poll_ns,
                   q->timeline.last_complete_ns, q->timeline.last_consume_ns,
                   d1, d2, d3, d4);
    }
    return 0;
}

static int stage10_timeline_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage10_timeline_show, inode->i_private);
}

static const struct file_operations stage10_timeline_fops = {
    .owner = THIS_MODULE,
    .open = stage10_timeline_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage10_vectors_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage10_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage10_vector *vec = &priv->vectors[i];
        seq_printf(m,
                   "vector%u: qid=%u target_cpu=%d "
                   "raise=%lld handle=%lld schedule=%lld "
                   "last_raise_cpu=%d last_handle_cpu=%d "
                   "last_raise_ns=%llu last_handle_ns=%llu\n",
                   vec->vector_id, vec->qid, vec->target_cpu,
                   atomic64_read(&vec->stats.raise_count),
                   atomic64_read(&vec->stats.handle_count),
                   atomic64_read(&vec->stats.schedule_count),
                   vec->last_raise_cpu, vec->last_handle_cpu,
                   vec->last_raise_ns, vec->last_handle_ns);
    }
    return 0;
}

static int stage10_vectors_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage10_vectors_show, inode->i_private);
}

static const struct file_operations stage10_vectors_fops = {
    .owner = THIS_MODULE,
    .open = stage10_vectors_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static void stage10_debugfs_init(struct stage10_priv *priv)
{
    priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
    if (!priv->dbg_dir)
        return;
    debugfs_create_file("stats", 0444, priv->dbg_dir, priv->ndev, &stage10_stats_fops);
    debugfs_create_file("queues", 0444, priv->dbg_dir, priv->ndev, &stage10_queues_fops);
    debugfs_create_file("timeline", 0444, priv->dbg_dir, priv->ndev, &stage10_timeline_fops);
    debugfs_create_file("vectors", 0444, priv->dbg_dir, priv->ndev, &stage10_vectors_fops);
}

static void stage10_debugfs_deinit(struct stage10_priv *priv)
{
    debugfs_remove_recursive(priv->dbg_dir);
    priv->dbg_dir = NULL;
}

/*========================================================
 *     init / exit
 *========================================================*/
static int __init stage10_soft_init(void)
{
    struct net_device *ndev;
    struct stage10_priv *priv;
    int i, ret;

    num_queues = clamp_t(unsigned int, num_queues, 1, STAGE10_MAX_QUEUES);
    ring_size = max_t(unsigned int, ring_size, 32);
    napi_weight = max_t(unsigned int, napi_weight, 16);
    backend_batch = max_t(unsigned int, backend_batch, 1);

    ndev = alloc_etherdev_mqs(sizeof(struct stage10_priv), num_queues, num_queues);
    if (!ndev)
        return -ENOMEM;

    strscpy(ndev->name, ifname, IFNAMSIZ);
    ndev->netdev_ops = &stage10_netdev_ops;
    ndev->needs_free_netdev = true;
    eth_hw_addr_random(ndev);

    priv = netdev_priv(ndev);
    memset(priv, 0, sizeof(*priv));
    priv->ndev = ndev;
    priv->num_queues = num_queues;
    priv->ring_size = ring_size;
    priv->napi_weight = napi_weight;
    priv->rx_buf_size = rx_buf_size;
    priv->backend_delay_us = backend_delay_us;
    priv->backend_batch = backend_batch;
    spin_lock_init(&priv->state_lock);

    priv->backend_wq = alloc_workqueue("stage10s_backend", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!priv->backend_wq) {
        free_netdev(ndev);
        return -ENOMEM;
    }
    priv->irq_wq = alloc_workqueue("stage10s_irq", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!priv->irq_wq) {
        destroy_workqueue(priv->backend_wq);
        free_netdev(ndev);
        return -ENOMEM;
    }

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage10_queue *q = &priv->queues[i];
        q->priv = priv;
        q->qid = i;
        q->vector_id = i;
        INIT_WORK(&q->backend_work, stage10_backend_workfn);
        INIT_WORK(&q->irq_work, stage10_irq_workfn);
        ret = stage10_alloc_ring(&q->txq, ring_size);
        if (ret)
            goto err;
        ret = stage10_alloc_ring(&q->rxq, ring_size);
        if (ret)
            goto err;
        STAGE10_NETIF_NAPI_ADD(ndev, &q->napi, stage10_napi_poll, napi_weight);

        /* vector 初始化 */
        priv->vectors[i].vector_id = i;
        priv->vectors[i].qid = i;
        priv->vectors[i].target_cpu = stage10_pick_irq_cpu(i);
        snprintf(priv->vectors[i].name, sizeof(priv->vectors[i].name), "stage10s-q%u", i);
        priv->vectors[i].last_raise_cpu = -1;
        priv->vectors[i].last_handle_cpu = -1;

        stage10_reset_queue(q);
    }

    ret = register_netdev(ndev);
    if (ret)
        goto err;

    stage10_debugfs_init(priv);
    stage10_soft_ndev = ndev;
    pr_info("%s: loaded ifname=%s num_queues=%u ring_size=%u napi_weight=%u backend_batch=%u\n",
            DRV_NAME, ndev->name, priv->num_queues, priv->ring_size,
            priv->napi_weight, priv->backend_batch);
    return 0;
err:
    for (i = 0; i < priv->num_queues; ++i) {
        if (priv->queues[i].napi.dev)
            netif_napi_del(&priv->queues[i].napi);
        stage10_free_ring(ndev, &priv->queues[i].txq, false);
        stage10_free_ring(ndev, &priv->queues[i].rxq, true);
    }
    if (priv->irq_wq)
        destroy_workqueue(priv->irq_wq);
    destroy_workqueue(priv->backend_wq);
    free_netdev(ndev);
    return ret;
}

static void __exit stage10_soft_exit(void)
{
    struct stage10_priv *priv;
    int i;

    if (!stage10_soft_ndev)
        return;
    priv = netdev_priv(stage10_soft_ndev);
    stage10_debugfs_deinit(priv);
    unregister_netdev(stage10_soft_ndev);
    flush_workqueue(priv->backend_wq);
    flush_workqueue(priv->irq_wq);
    destroy_workqueue(priv->backend_wq);
    destroy_workqueue(priv->irq_wq);
    for (i = 0; i < priv->num_queues; ++i) {
        cancel_work_sync(&priv->queues[i].backend_work);
        cancel_work_sync(&priv->queues[i].irq_work);
        netif_napi_del(&priv->queues[i].napi);
        stage10_free_ring(stage10_soft_ndev, &priv->queues[i].txq, false);
        stage10_free_ring(stage10_soft_ndev, &priv->queues[i].rxq, true);
    }
    free_netdev(stage10_soft_ndev);
    stage10_soft_ndev = NULL;
    pr_info("%s: unloaded\n", DRV_NAME);
}

module_init(stage10_soft_init);
module_exit(stage10_soft_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Lab");
MODULE_DESCRIPTION("stage10 soft MSI-X teaching netdev");