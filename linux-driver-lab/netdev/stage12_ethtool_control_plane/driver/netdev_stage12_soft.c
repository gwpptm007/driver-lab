// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage12_soft.c — stage12 纯软教学模型
 *
 * 整合方案：每队列独立 page_pool + build_skb() 零拷贝 RX + ethtool 控制面
 *
 * 架构要点：
 *   1. 每队列独立 page_pool（真实驱动模式）
 *   2. RX: build_skb() 从 page 直接构建 skb（零拷贝）
 *   3. 成功后不显式 recycle — skb destructor 的 put_page 自动归 page 回 pool
 *   4. 失败时 page_pool_recycle_direct() 显式回收
 *   5. 5状态 RX slot: FREE → POSTED → READY → DONE → FREE
 *   6. MSI-X 语义模拟: vector/irq_work/backend_work 链
 *   7. ethtool_ops: stats导出 / ringparam / channels / priv_flags
 *
 * 上下文：Linux kernel netdev子系统
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <net/page_pool/helpers.h>

#include "../include/netdev_stage12_compat.h"

#define DRV_NAME    "netdev_stage12_soft"
#define STAGE12_MAX_QUEUES      4
#define STAGE12_DEFAULT_NUM_QUEUES  2
#define STAGE12_DEFAULT_RING_SIZE  128
#define STAGE12_DEFAULT_NAPI_WEIGHT 64
#define STAGE12_DEFAULT_BACKEND_BATCH 64
#define STAGE12_DEFAULT_BACKEND_DELAY_US 0
#define STAGE12_QUEUE_DUMP_LIMIT 8

#define STAGE12_TEST_PROTO  0x88BA
#define STAGE12_TEST_MAGIC  "STAGE12"

/*========================================================
 *     底层数据结构
 *========================================================*/

/* slot 状态机 — 5 状态
 *
 * TX path: FREE → SUBMITTED → DONE → FREE
 * RX path: FREE → POSTED → READY → DONE → FREE
 */
enum stage12_slot_state {
    S12_SLOT_FREE = 0,
    S12_SLOT_POSTED,   /* RX: page 已分配，等待 backend 填充 */
    S12_SLOT_SUBMITTED, /* TX: skb 已提交，等待 backend 处理 */
    S12_SLOT_READY,    /* RX: backend 已填充数据，等待 napi 消费 */
    S12_SLOT_DONE,     /* TX: 传输完成 / RX: 已消费，等待 refill */
};

/* 描述符（存放元数据，不存放数据） */
struct stage12_desc {
    u32 data_len;
    u16 state;
    u16 flags;
};

/* Buffer slot — 使用 page（来自 page_pool） */
struct stage12_buf_slot {
    struct page *page;      /* 来自 page_pool 的 page */
    void *buf;               /* page_address(page)，用于 bounce copy */
    u16 buf_len;
    u16 data_len;
    enum stage12_slot_state state;
    u16 id;
    u32 last_seq;
};

/* TX/RX ring */
struct stage12_ring {
    struct stage12_desc  *desc;
    struct stage12_buf_slot *slots;
    u16 size;

    /* TX indices */
    u16 submit_idx;
    u16 notify_idx;
    u16 complete_idx;

    /* RX indices */
    u16 post_idx;       /* 下一个可用的 posted slot */
    u16 device_idx;     /* backend 生产位置（填充 RX page） */
    u16 consume_idx;    /* napi 消费位置 */
};

/* Per-queue timeline */
struct stage12_timeline {
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
 *========================================================*/
struct stage12_vector_stats {
    atomic64_t raise_count;
    atomic64_t handle_count;
    atomic64_t schedule_count;
};

struct stage12_vector {
    u16 vector_id;
    u16 qid;
    int target_cpu;
    char name[32];
    struct stage12_vector_stats stats;
    u64 last_raise_ns;
    u64 last_handle_ns;
    int last_raise_cpu;
    int last_handle_cpu;
};

/* Per-queue 统计 */
struct stage12_queue_stats {
    atomic64_t tx_submit_count;
    atomic64_t tx_complete_count;
    atomic64_t tx_packets;
    atomic64_t tx_bytes;
    atomic64_t tx_busy;
    atomic64_t tx_dropped;
    atomic64_t tx_linearize_count;

    atomic64_t rx_post_count;
    atomic64_t rx_ready_count;
    atomic64_t rx_consume_count;
    atomic64_t rx_packets;
    atomic64_t rx_bytes;
    atomic64_t rx_dropped;

    /* page_pool 统计 */
    atomic64_t pp_alloc;          /* page_pool_dev_alloc_pages 成功次数 */
    atomic64_t pp_recycle;         /* put_page 归 page 回 pool 次数（仅失败路径） */
    atomic64_t pp_build_skb_fail;  /* build_skb 失败次数 */

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
struct stage12_queue {
    struct stage12_priv *priv;
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
    u16 rx_posted;   /* 已分配 page 待消费 */
    u16 rx_ready;    /* 数据就绪等待消费 */

    /* rings */
    struct stage12_ring txq;
    struct stage12_ring rxq;

    /* timeline */
    struct stage12_timeline timeline;

    /* 统计 */
    struct stage12_queue_stats stats;

    /* 每队列独立 page_pool（核心设计） */
    struct page_pool *pp;
};

/* priv */
struct stage12_priv {
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
    u32 ethtool_priv_flags;

    atomic64_t rr_counter;
    atomic64_t open_count;
    atomic64_t stop_count;

    /* per-queue vectors */
    struct stage12_vector vectors[STAGE12_MAX_QUEUES];

    /* per-queue 上下文 */
    struct stage12_queue queues[STAGE12_MAX_QUEUES];
};

/*========================================================
 *     模块参数
 *========================================================*/
static char ifname[IFNAMSIZ] = "nds12s";
module_param_string(ifname, ifname, sizeof(ifname), 0444);
MODULE_PARM_DESC(ifname, "network interface name");

static unsigned int num_queues = STAGE12_DEFAULT_NUM_QUEUES;
module_param(num_queues, uint, 0444);
MODULE_PARM_DESC(num_queues, "number of TX/RX queue pairs");

static unsigned int ring_size = STAGE12_DEFAULT_RING_SIZE;
module_param(ring_size, uint, 0444);
MODULE_PARM_DESC(ring_size, "descriptor ring size");

static unsigned int napi_weight = STAGE12_DEFAULT_NAPI_WEIGHT;
module_param(napi_weight, uint, 0444);
MODULE_PARM_DESC(napi_weight, "NAPI poll budget");

static unsigned int backend_delay_us = STAGE12_DEFAULT_BACKEND_DELAY_US;
module_param(backend_delay_us, uint, 0644);
MODULE_PARM_DESC(backend_delay_us, "simulated backend delay in microseconds");

static unsigned int backend_batch = STAGE12_DEFAULT_BACKEND_BATCH;
module_param(backend_batch, uint, 0444);
MODULE_PARM_DESC(backend_batch, "backend batch size");

static unsigned int rx_buf_size = 2048;
module_param(rx_buf_size, uint, 0444);
MODULE_PARM_DESC(rx_buf_size, "RX buffer size");

/* 全局 ndev 指针 */
static struct net_device *stage12_soft_ndev;

/*========================================================
 *     时间戳
 *========================================================*/
static inline u64 stage12_now_ns(void)
{
    return ktime_get_ns();
}

/*========================================================
 *     工具函数
 *========================================================*/
static inline u16 stage12_next_idx(u16 idx, u16 size)
{
    return (idx + 1) % size;
}

/* 选择 vector 对应的 CPU（round-robin） */
static int stage12_pick_irq_cpu(u16 qid)
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

static struct stage12_vector *stage12_get_vector(struct stage12_priv *priv, u16 qid)
{
    if (qid >= priv->num_queues)
        return NULL;
    return &priv->vectors[qid];
}

static inline bool stage12_is_test_frame(struct sk_buff *skb)
{
    if (ntohs(skb->protocol) != STAGE12_TEST_PROTO)
        return false;
    if (skb_headlen(skb) < ETH_HLEN + 8)
        return false;
    return memcmp(skb->data + ETH_HLEN, STAGE12_TEST_MAGIC, 7) == 0;
}

/*========================================================
 *     page_pool 初始化 / 销毁（每队列独立）
 *========================================================*/
static struct page_pool *stage12_create_page_pool(struct stage12_queue *q)
{
    struct page_pool_params params = {
        .order = 0,
        .pool_size = q->priv->ring_size * 2,
        .nid = NUMA_NO_NODE,
        .dev = q->priv->ndev->dev.parent,
        .dma_dir = DMA_FROM_DEVICE,
    };

    return page_pool_create(&params);
}

static void stage12_destroy_page_pool(struct stage12_queue *q)
{
    if (q->pp) {
        page_pool_destroy(q->pp);
        q->pp = NULL;
    }
}

/*========================================================
 *     内存分配 / 释放
 *========================================================*/
static int stage12_alloc_ring(struct stage12_ring *r, u16 size)
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

static void stage12_free_ring(struct net_device *ndev, struct stage12_ring *r, bool is_rx)
{
    u16 i;
    if (!r->slots)
        goto out;
    for (i = 0; i < r->size; ++i) {
        struct stage12_buf_slot *s = &r->slots[i];
        if (s->page) {
            /* RX: 释放 page 回 page_pool */
            if (is_rx)
                put_page(s->page);
            s->page = NULL;
        }
        kfree(s->buf);
        s->buf = NULL;
    }
out:
    kfree(r->slots);
    kfree(r->desc);
    memset(r, 0, sizeof(*r));
}

static void stage12_reset_queue(struct stage12_queue *q)
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
 *     RX buffer 管理 — page_pool 版本
 *========================================================*/

/* 释放 RX slot 中的 page（经过 page_pool 回收链路） */
static void stage12_release_rx_page(struct stage12_queue *q,
                                    struct stage12_buf_slot *slot)
{
    if (!slot->page)
        return;
    /* 绕过 page_pool，直接 put_page 归 page 回 pool */
    put_page(slot->page);
    atomic64_inc(&q->stats.pp_recycle);
    slot->page = NULL;
    slot->buf = NULL;
    slot->data_len = 0;
    slot->last_seq = 0;
    slot->state = S12_SLOT_FREE;
}

/* 为 RX slot 补充一个 page（从 page_pool 分配） */
static int stage12_refill_rx_slot(struct stage12_queue *q, u16 idx)
{
    struct stage12_priv *priv = q->priv;
    struct stage12_buf_slot *slot = &q->rxq.slots[idx];
    struct page *page;

    if (slot->page)
        return 0;

    page = page_pool_dev_alloc_pages(q->pp);
    if (!page)
        return -ENOMEM;

    slot->page = page;
    slot->buf = page_address(page);
    slot->buf_len = priv->rx_buf_size;
    slot->data_len = 0;
    slot->last_seq = 0;
    slot->state = S12_SLOT_POSTED;
    q->rx_posted++;
    atomic64_inc(&q->stats.pp_alloc);
    atomic64_inc(&q->stats.rx_post_count);
    return 0;
}

/*========================================================
 *     MSI-X 语义模拟：irq_workfn
 *========================================================*/
static void stage12_irq_workfn(struct work_struct *work)
{
    struct stage12_queue *q = container_of(work, struct stage12_queue, irq_work);
    struct stage12_priv *priv = q->priv;
    struct stage12_vector *vec = stage12_get_vector(priv, q->qid);
    unsigned long flags;

    spin_lock_irqsave(&priv->state_lock, flags);
    q->timeline.last_irq_ns = stage12_now_ns();
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
 *========================================================*/
static void stage12_raise_irq(struct stage12_queue *q)
{
    struct stage12_priv *priv = q->priv;
    struct stage12_vector *vec = stage12_get_vector(priv, q->qid);

    if (vec) {
        atomic64_inc(&vec->stats.raise_count);
        atomic64_inc(&vec->stats.schedule_count);
        vec->last_raise_ns = stage12_now_ns();
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
static void stage12_mark_doorbell(struct stage12_queue *q)
{
    q->timeline.last_doorbell_ns = stage12_now_ns();
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
static void stage12_complete_tx_one(struct stage12_queue *q)
{
    struct stage12_ring *r = &q->txq;
    struct stage12_desc *d = &r->desc[r->complete_idx];
    struct stage12_buf_slot *s = &r->slots[r->complete_idx];

    if (!q->tx_done || s->state != S12_SLOT_DONE)
        return;

    /* soft model: TX slot 使用 bounce buffer（kmalloc） */
    kfree(s->buf);
    s->buf = NULL;
    s->page = NULL;
    memset(s, 0, sizeof(*s));
    memset(d, 0, sizeof(*d));
    r->complete_idx = stage12_next_idx(r->complete_idx, r->size);
    q->tx_done--;
    q->tx_inflight--;
    q->timeline.last_complete_ns = stage12_now_ns();
    atomic64_inc(&q->stats.tx_complete_count);
}

/*========================================================
 *     RX consume（在 NAPI poll 中）— build_skb 零拷贝版本
 *
 *     使用 build_skb() 从 page 构建 skb：
 *       - 成功：skb destructor 的 put_page 自动归 page 回 pool
 *       - 失败：page_pool_recycle_direct 显式回收
 *     消费后立即 refill，保持 posted 水位。
 *
 *     CHANGELOG:
 *     2026-04-20: napi_build_skb → build_skb
 *         解决 "Bad page state" warning：
 *         napi_build_skb 内部调用 get_page(refcount 1→2)，
 *         soft 模型下 page_pool inflight tracking 与双 put_page 路径冲突。
 *         build_skb 不调用 get_page，refcount 保持 1，
 *         destructor single put_page 即可归池，路径更简单。
 *========================================================*/
static int stage12_consume_rx_one(struct stage12_queue *q)
{
    struct stage12_priv *priv = q->priv;
    struct net_device *ndev = priv->ndev;
    struct stage12_ring *r = &q->rxq;
    struct stage12_desc *d = &r->desc[r->consume_idx];
    struct stage12_buf_slot *s = &r->slots[r->consume_idx];
    struct page *page = s->page;
    void *buf = s->buf;
    u32 len = s->data_len;
    struct sk_buff *skb;
    u16 idx = r->consume_idx;

    if (!q->rx_ready || s->state != S12_SLOT_READY || !page)
        return 0;

    /* 使用 build_skb() 从 page 构建 skb
     * build_skb 不调用 get_page，page refcount 保持 1
     * 成功后 skb destructor 的 put_page 将 refcount 1→0，page 正确归池
     * 失败时 page refcount=1，直接 page_pool_recycle_direct 归还
     */
    skb = build_skb(buf, priv->rx_buf_size);
    if (!skb) {
        /* build_skb 失败：page 未被使用，直接回收 */
        page_pool_recycle_direct(q->pp, page);
        atomic64_inc(&q->stats.pp_build_skb_fail);
        /* slot 清理 */
        memset(s, 0, sizeof(*s));
        memset(d, 0, sizeof(*d));
        r->consume_idx = stage12_next_idx(r->consume_idx, r->size);
        q->rx_ready--;
        /* 立即 refill */
        stage12_refill_rx_slot(q, idx);
        return 0;
    }

    skb_put(skb, len);
    skb->protocol = eth_type_trans(skb, ndev);
    if (stage12_is_test_frame(skb))
        atomic64_inc(&q->stats.test_rx_consume_count);

    netif_receive_skb(skb);
    /* build_skb 成功：destructor put_page 将 refcount 1→0，page 归池
     * 不需要显式 recycle */

    /* slot 清理（page 已由 skb destructor 持有） */
    memset(s, 0, sizeof(*s));
    memset(d, 0, sizeof(*d));
    r->consume_idx = stage12_next_idx(r->consume_idx, r->size);
    q->rx_ready--;
    q->timeline.last_consume_ns = stage12_now_ns();
    atomic64_inc(&q->stats.rx_consume_count);
    atomic64_inc(&q->stats.rx_packets);
    atomic64_add(len, &q->stats.rx_bytes);

    /* 立即 refill，保持 posted 水位 */
    stage12_refill_rx_slot(q, idx);
    return 1;
}

/*========================================================
 *     backend workfn — 异步处理 TX/RX
 *========================================================*/
static void stage12_backend_workfn(struct work_struct *work)
{
    struct stage12_queue *q = container_of(work, struct stage12_queue, backend_work);
    struct stage12_priv *priv = q->priv;
    unsigned long flags;
    int processed = 0;
    bool need_resched = false;

    if (priv->backend_delay_us)
        usleep_range(priv->backend_delay_us, priv->backend_delay_us + 50);

    spin_lock_irqsave(&priv->state_lock, flags);
    q->doorbell_pending = false;
    q->backend_running = true;
    q->timeline.last_backend_wakeup_ns = stage12_now_ns();
    atomic64_inc(&q->stats.backend_run_count);

    while (processed < priv->backend_batch) {
        struct stage12_ring *txr = &q->txq;
        struct stage12_ring *rxr = &q->rxq;
        struct stage12_desc *txd, *rxd;
        struct stage12_buf_slot *txs, *rxs;
        u16 txi, rxi;
        u32 copy_len;

        /* TX: 没有新提交的 TX */
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

        if (txs->state != S12_SLOT_SUBMITTED)
            break;
        if (rxs->state != S12_SLOT_POSTED)
            break;

        /* 模拟 backend 收到 TX 数据并转发到 RX */
        copy_len = min(txd->data_len, rxs->buf_len);
        memcpy(rxs->buf, txs->buf, copy_len);
        rxs->data_len = copy_len;
        rxs->last_seq = txs->last_seq;
        rxs->state = S12_SLOT_READY;   /* 状态变为 READY */
        rxd->data_len = copy_len;
        rxd->state = S12_SLOT_READY;
        rxr->device_idx = stage12_next_idx(rxr->device_idx, rxr->size);
        q->rx_posted--;
        q->rx_ready++;
        atomic64_inc(&q->stats.rx_ready_count);

        txs->state = S12_SLOT_DONE;
        txd->state = S12_SLOT_DONE;
        txr->notify_idx = stage12_next_idx(txr->notify_idx, txr->size);
        q->tx_done++;

        atomic64_inc(&q->stats.backend_tx_processed);
        atomic64_inc(&q->stats.backend_rx_produced);
        processed++;
    }

    q->timeline.last_backend_done_ns = stage12_now_ns();
    /* 触发 MSI-X 语义模拟 */
    if (processed)
        stage12_raise_irq(q);
    if (q->txq.notify_idx != q->txq.submit_idx)
        need_resched = true;
    q->backend_running = false;
    spin_unlock_irqrestore(&priv->state_lock, flags);

    if (need_resched)
        stage12_mark_doorbell(q);
}

/*========================================================
 *     NAPI poll — 消费 TX done 和 RX ready
 *========================================================*/
static int stage12_napi_poll(struct napi_struct *napi, int budget)
{
    struct stage12_queue *q = container_of(napi, struct stage12_queue, napi);
    struct stage12_priv *priv = q->priv;
    unsigned long flags;
    int work = 0;

    spin_lock_irqsave(&priv->state_lock, flags);
    q->timeline.last_poll_ns = stage12_now_ns();
    atomic64_inc(&q->stats.napi_poll_count);

    /* TX complete */
    while (q->tx_done)
        stage12_complete_tx_one(q);

    /* RX consume */
    while (q->rx_ready && work < budget)
        work += stage12_consume_rx_one(q);

    atomic64_add(work, &q->stats.napi_work_total);
    if (!q->rx_ready && !q->tx_done) {
        STAGE12_NAPI_COMPLETE(napi, work);
        q->irq_masked = false;
        atomic64_inc(&q->stats.napi_complete_count);
        if (q->doorbell_pending || q->txq.notify_idx != q->txq.submit_idx)
            stage12_mark_doorbell(q);
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);
    return work;
}

/*========================================================
 *     ndo_start_xmit
 *========================================================*/
static netdev_tx_t stage12_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    struct stage12_queue *q;
    struct stage12_ring *r;
    struct stage12_desc *d;
    struct stage12_buf_slot *s;
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
    if (s->state != S12_SLOT_FREE) {
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

    s->buf = buf;
    s->buf_len = skb_headlen(skb);
    s->data_len = skb_headlen(skb);
    s->state = S12_SLOT_SUBMITTED;
    s->id = idx;
    d->data_len = skb_headlen(skb);
    d->state = S12_SLOT_SUBMITTED;
    r->submit_idx = stage12_next_idx(r->submit_idx, r->size);
    q->tx_inflight++;

    q->timeline.last_submit_ns = stage12_now_ns();
    atomic64_inc(&q->stats.tx_submit_count);
    atomic64_inc(&q->stats.tx_packets);
    atomic64_add(skb_headlen(skb), &q->stats.tx_bytes);
    if (stage12_is_test_frame(skb))
        atomic64_inc(&q->stats.test_tx_submit_count);

    stage12_mark_doorbell(q);
    spin_unlock_irqrestore(&priv->state_lock, flags);
    return NETDEV_TX_OK;
}

/*========================================================
 *     ndo_select_queue
 *========================================================*/
static u16 stage12_select_queue(struct net_device *ndev, struct sk_buff *skb,
                                 struct net_device *sb_dev)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    u32 hash = skb_get_hash(skb);

    if (hash)
        return reciprocal_scale(hash, priv->num_queues);
    return atomic64_inc_return(&priv->rr_counter) % priv->num_queues;
}

/*========================================================
 *     ndo_open / ndo_stop
 *========================================================*/
static int stage12_open(struct net_device *ndev)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    unsigned long flags;
    int i;

    spin_lock_irqsave(&priv->state_lock, flags);
    for (i = 0; i < priv->num_queues; ++i) {
        stage12_reset_queue(&priv->queues[i]);
        /* 预填充所有 RX slot */
        while (priv->queues[i].rx_posted < priv->queues[i].rxq.size - 1) {
            if (stage12_refill_rx_slot(&priv->queues[i],
                                        priv->queues[i].rxq.post_idx) != 0)
                break;
            priv->queues[i].rxq.post_idx = stage12_next_idx(
                priv->queues[i].rxq.post_idx, priv->queues[i].rxq.size);
        }
        napi_enable(&priv->queues[i].napi);
    }
    atomic64_inc(&priv->open_count);
    spin_unlock_irqrestore(&priv->state_lock, flags);

    netif_tx_start_all_queues(ndev);
    return 0;
}

static int stage12_stop(struct net_device *ndev)
{
    struct stage12_priv *priv = netdev_priv(ndev);
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
static void stage12_get_stats64(struct net_device *ndev, struct rtnl_link_stats64 *stats)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage12_queue *q = &priv->queues[i];
        stats->tx_packets += atomic64_read(&q->stats.tx_packets);
        stats->tx_bytes += atomic64_read(&q->stats.tx_bytes);
        stats->tx_dropped += atomic64_read(&q->stats.tx_dropped);
        stats->rx_packets += atomic64_read(&q->stats.rx_packets);
        stats->rx_bytes += atomic64_read(&q->stats.rx_bytes);
        stats->rx_dropped += atomic64_read(&q->stats.rx_dropped);
    }
}

/*========================================================
 *     ethtool_ops 实现
 *========================================================*/

/* Ethtool stats - 标准 ethtool 接口 */
enum {
    STAGE12_ETHTOOL_STATS_TX_PACKETS,
    STAGE12_ETHTOOL_STATS_TX_BYTES,
    STAGE12_ETHTOOL_STATS_TX_SUBMIT,
    STAGE12_ETHTOOL_STATS_TX_COMPLETE,
    STAGE12_ETHTOOL_STATS_TX_DROPPED,
    STAGE12_ETHTOOL_STATS_RX_PACKETS,
    STAGE12_ETHTOOL_STATS_RX_BYTES,
    STAGE12_ETHTOOL_STATS_RX_CONSUME,
    STAGE12_ETHTOOL_STATS_RX_DROPPED,
    STAGE12_ETHTOOL_STATS_RX_PAGE_ALLOC,
    STAGE12_ETHTOOL_STATS_RX_BUILD_SKB_FAIL,
    STAGE12_ETHTOOL_STATS_TEST_TX_SUBMIT,
    STAGE12_ETHTOOL_STATS_TEST_RX_CONSUME,
    STAGE12_ETHTOOL_STATS_COUNT,
};

static const char stage12_ethtool_stat_names[][ETH_GSTRING_LEN] = {
    [STAGE12_ETHTOOL_STATS_TX_PACKETS]       = "tx_packets",
    [STAGE12_ETHTOOL_STATS_TX_BYTES]         = "tx_bytes",
    [STAGE12_ETHTOOL_STATS_TX_SUBMIT]        = "tx_submit_count",
    [STAGE12_ETHTOOL_STATS_TX_COMPLETE]      = "tx_complete_count",
    [STAGE12_ETHTOOL_STATS_TX_DROPPED]       = "tx_dropped",
    [STAGE12_ETHTOOL_STATS_RX_PACKETS]       = "rx_packets",
    [STAGE12_ETHTOOL_STATS_RX_BYTES]         = "rx_bytes",
    [STAGE12_ETHTOOL_STATS_RX_CONSUME]       = "rx_consume_count",
    [STAGE12_ETHTOOL_STATS_RX_DROPPED]       = "rx_dropped",
    [STAGE12_ETHTOOL_STATS_RX_PAGE_ALLOC]    = "rx_page_alloc",
    [STAGE12_ETHTOOL_STATS_RX_BUILD_SKB_FAIL] = "rx_build_skb_fail",
    [STAGE12_ETHTOOL_STATS_TEST_TX_SUBMIT]   = "test_tx_submit",
    [STAGE12_ETHTOOL_STATS_TEST_RX_CONSUME]  = "test_rx_consume",
};

static void stage12_get_drvinfo(struct net_device *ndev,
                                struct ethtool_drvinfo *drvinfo)
{
    strscpy(drvinfo->driver, "netdev_stage12", sizeof(drvinfo->driver));
    strscpy(drvinfo->version, "1.0", sizeof(drvinfo->version));
    strscpy(drvinfo->bus_info, "platform", sizeof(drvinfo->bus_info));
}

static void stage12_get_strings(struct net_device *ndev, u32 stringset, u8 *buf)
{
    if (stringset == ETH_SS_STATS)
        memcpy(buf, stage12_ethtool_stat_names,
               sizeof(stage12_ethtool_stat_names));
}

static int stage12_get_sset_count(struct net_device *ndev, int sset)
{
    if (sset == ETH_SS_STATS)
        return STAGE12_ETHTOOL_STATS_COUNT;
    return -EOPNOTSUPP;
}

static void stage12_get_ethtool_stats(struct net_device *ndev,
                                      struct ethtool_stats *stats,
                                      u64 *data)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    int i;

    memset(data, 0, sizeof(u64) * STAGE12_ETHTOOL_STATS_COUNT);

    for (i = 0; i < priv->num_queues; i++) {
        struct stage12_queue *q = &priv->queues[i];

        /* 聚合统计：全部累加 */
        data[STAGE12_ETHTOOL_STATS_TX_PACKETS] += atomic64_read(&q->stats.tx_packets);
        data[STAGE12_ETHTOOL_STATS_TX_BYTES]  += atomic64_read(&q->stats.tx_bytes);
        data[STAGE12_ETHTOOL_STATS_TX_SUBMIT] += atomic64_read(&q->stats.tx_submit_count);
        data[STAGE12_ETHTOOL_STATS_TX_COMPLETE] += atomic64_read(&q->stats.tx_complete_count);
        data[STAGE12_ETHTOOL_STATS_TX_DROPPED] += atomic64_read(&q->stats.tx_dropped);

        data[STAGE12_ETHTOOL_STATS_RX_PACKETS] += atomic64_read(&q->stats.rx_packets);
        data[STAGE12_ETHTOOL_STATS_RX_BYTES]  += atomic64_read(&q->stats.rx_bytes);
        data[STAGE12_ETHTOOL_STATS_RX_CONSUME] += atomic64_read(&q->stats.rx_consume_count);
        data[STAGE12_ETHTOOL_STATS_RX_DROPPED] += atomic64_read(&q->stats.rx_dropped);

        data[STAGE12_ETHTOOL_STATS_RX_PAGE_ALLOC] += atomic64_read(&q->stats.pp_alloc);
        data[STAGE12_ETHTOOL_STATS_RX_BUILD_SKB_FAIL] += atomic64_read(&q->stats.pp_build_skb_fail);

        data[STAGE12_ETHTOOL_STATS_TEST_TX_SUBMIT] += atomic64_read(&q->stats.test_tx_submit_count);
        data[STAGE12_ETHTOOL_STATS_TEST_RX_CONSUME] += atomic64_read(&q->stats.test_rx_consume_count);
    }
}

static void stage12_get_ringparam(struct net_device *ndev,
                                  struct ethtool_ringparam *ringparam,
                                  struct kernel_ethtool_ringparam *kernel_ringparam,
                                  struct netlink_ext_ack *extack)
{
    struct stage12_priv *priv = netdev_priv(ndev);

    ringparam->rx_max_pending = priv->ring_size;
    ringparam->rx_pending = priv->ring_size;
    ringparam->tx_max_pending = priv->ring_size;
    ringparam->tx_pending = priv->ring_size;
}

static int stage12_set_ringparam(struct net_device *ndev,
                                 struct ethtool_ringparam *ringparam,
                                 struct kernel_ethtool_ringparam *kernel_ringparam,
                                 struct netlink_ext_ack *extack)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    u32 new_size = ringparam->rx_pending;

    if (new_size > priv->ring_size || new_size < 64) {
        netdev_info(ndev, "Ringparam: requested %u out of range [64, %u]\n",
                    new_size, priv->ring_size);
        return -EINVAL;
    }

    netdev_info(ndev, "Ringparam: requested %u, current %u (runtime change not supported)\n",
                new_size, priv->ring_size);
    return 0;
}

static void stage12_get_channels(struct net_device *ndev,
                                 struct ethtool_channels *channels)
{
    struct stage12_priv *priv = netdev_priv(ndev);

    channels->max_rx = priv->num_queues;
    channels->max_tx = priv->num_queues;
    channels->rx_count = priv->num_queues;
    channels->tx_count = priv->num_queues;
}

static int stage12_set_channels(struct net_device *ndev,
                                 struct ethtool_channels *channels)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    u32 new_count = channels->rx_count;

    if (new_count > priv->num_queues || new_count < 1) {
        netdev_info(ndev, "Channels: requested %u out of range [1, %u]\n",
                    new_count, priv->num_queues);
        return -EINVAL;
    }

    netdev_info(ndev, "Channels: requested %u, current %u (runtime change not supported)\n",
                new_count, priv->num_queues);
    return 0;
}

static u32 stage12_get_priv_flags(struct net_device *ndev)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    return priv->ethtool_priv_flags;
}

static int stage12_set_priv_flags(struct net_device *ndev, u32 flags)
{
    struct stage12_priv *priv = netdev_priv(ndev);
    priv->ethtool_priv_flags = flags;
    return 0;
}

static const struct ethtool_ops stage12_ethtool_ops = {
    .get_drvinfo        = stage12_get_drvinfo,
    .get_strings        = stage12_get_strings,
    .get_sset_count     = stage12_get_sset_count,
    .get_ethtool_stats  = stage12_get_ethtool_stats,
    .get_ringparam      = stage12_get_ringparam,
    .set_ringparam      = stage12_set_ringparam,
    .get_channels       = stage12_get_channels,
    .set_channels       = stage12_set_channels,
    .get_priv_flags     = stage12_get_priv_flags,
    .set_priv_flags     = stage12_set_priv_flags,
    .get_link           = ethtool_op_get_link,
};

static const struct net_device_ops stage12_netdev_ops = {
    .ndo_open = stage12_open,
    .ndo_stop = stage12_stop,
    .ndo_start_xmit = stage12_start_xmit,
    .ndo_select_queue = stage12_select_queue,
    .ndo_get_stats64 = stage12_get_stats64,
};

/*========================================================
 *     debugfs 实现
 *========================================================*/
static int stage12_stats_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage12_priv *priv = netdev_priv(ndev);
    int i;

    seq_printf(m, "ifname=%s num_queues=%u ring_size=%u napi_weight=%u backend_batch=%u open=%lld stop=%lld\n",
               ndev->name, priv->num_queues, priv->ring_size, priv->napi_weight,
               priv->backend_batch, atomic64_read(&priv->open_count),
               atomic64_read(&priv->stop_count));
    for (i = 0; i < priv->num_queues; ++i) {
        struct stage12_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx_submit=%lld tx_complete=%lld tx_packets=%lld tx_bytes=%lld tx_busy=%lld tx_drop=%lld "
                   "rx_post=%lld rx_ready=%lld rx_consume=%lld rx_packets=%lld rx_bytes=%lld rx_drop=%lld "
                   "doorbell=%lld backend_run=%lld backend_tx=%lld backend_rx=%lld "
                   "irq=%lld napi_poll=%lld napi_complete=%lld napi_work=%lld test_tx=%lld test_rx=%lld "
                   "pp_alloc=%lld pp_recycle=%lld pp_build_skb_fail=%lld\n",
                   q->qid,
                   atomic64_read(&q->stats.tx_submit_count),
                   atomic64_read(&q->stats.tx_complete_count),
                   atomic64_read(&q->stats.tx_packets),
                   atomic64_read(&q->stats.tx_bytes),
                   atomic64_read(&q->stats.tx_busy),
                   atomic64_read(&q->stats.tx_dropped),
                   atomic64_read(&q->stats.rx_post_count),
                   atomic64_read(&q->stats.rx_ready_count),
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
                   atomic64_read(&q->stats.test_rx_consume_count),
                   atomic64_read(&q->stats.pp_alloc),
                   atomic64_read(&q->stats.pp_recycle),
                   atomic64_read(&q->stats.pp_build_skb_fail));
    }
    return 0;
}

static int stage12_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage12_stats_show, inode->i_private);
}

static const struct file_operations stage12_stats_fops = {
    .owner = THIS_MODULE,
    .open = stage12_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage12_queues_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage12_priv *priv = netdev_priv(ndev);
    int i, j;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage12_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx submit=%u notify=%u complete=%u inflight=%u done=%u | "
                   "rx post=%u device=%u consume=%u posted=%u ready=%u "
                   "irq_masked=%u doorbell_pending=%u backend_running=%u\n",
                   q->qid, q->txq.submit_idx, q->txq.notify_idx, q->txq.complete_idx,
                   q->tx_inflight, q->tx_done, q->rxq.post_idx, q->rxq.device_idx,
                   q->rxq.consume_idx, q->rx_posted, q->rx_ready,
                   q->irq_masked, q->doorbell_pending, q->backend_running);
        for (j = 0; j < min_t(u16, q->txq.size, STAGE12_QUEUE_DUMP_LIMIT); ++j)
            seq_printf(m, "  q%u txslot[%d]: state=%u len=%u\n", q->qid, j,
                       q->txq.slots[j].state, q->txq.slots[j].data_len);
        for (j = 0; j < min_t(u16, q->rxq.size, STAGE12_QUEUE_DUMP_LIMIT); ++j)
            seq_printf(m, "  q%u rxslot[%d]: state=%u len=%u has_page=%u\n", q->qid, j,
                       q->rxq.slots[j].state, q->rxq.slots[j].data_len,
                       q->rxq.slots[j].page ? 1 : 0);
    }
    return 0;
}

static int stage12_queues_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage12_queues_show, inode->i_private);
}

static const struct file_operations stage12_queues_fops = {
    .owner = THIS_MODULE,
    .open = stage12_queues_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage12_timeline_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage12_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage12_queue *q = &priv->queues[i];
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

static int stage12_timeline_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage12_timeline_show, inode->i_private);
}

static const struct file_operations stage12_timeline_fops = {
    .owner = THIS_MODULE,
    .open = stage12_timeline_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage12_vectors_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage12_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage12_vector *vec = &priv->vectors[i];
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

static int stage12_vectors_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage12_vectors_show, inode->i_private);
}

static const struct file_operations stage12_vectors_fops = {
    .owner = THIS_MODULE,
    .open = stage12_vectors_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage12_pp_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage12_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage12_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: pool=%p pp_alloc=%lld pp_recycle=%lld pp_build_skb_fail=%lld posted=%u ready=%u\n",
                   q->qid, q->pp,
                   atomic64_read(&q->stats.pp_alloc),
                   atomic64_read(&q->stats.pp_recycle),
                   atomic64_read(&q->stats.pp_build_skb_fail),
                   q->rx_posted, q->rx_ready);
    }
    return 0;
}

static int stage12_pp_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage12_pp_show, inode->i_private);
}

static const struct file_operations stage12_pp_fops = {
    .owner = THIS_MODULE,
    .open = stage12_pp_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static void stage12_debugfs_init(struct stage12_priv *priv)
{
    priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
    if (!priv->dbg_dir)
        return;
    debugfs_create_file("stats", 0444, priv->dbg_dir, priv->ndev, &stage12_stats_fops);
    debugfs_create_file("queues", 0444, priv->dbg_dir, priv->ndev, &stage12_queues_fops);
    debugfs_create_file("timeline", 0444, priv->dbg_dir, priv->ndev, &stage12_timeline_fops);
    debugfs_create_file("vectors", 0444, priv->dbg_dir, priv->ndev, &stage12_vectors_fops);
    debugfs_create_file("page_pool", 0444, priv->dbg_dir, priv->ndev, &stage12_pp_fops);
}

static void stage12_debugfs_deinit(struct stage12_priv *priv)
{
    debugfs_remove_recursive(priv->dbg_dir);
    priv->dbg_dir = NULL;
}

/*========================================================
 *     init / exit
 *========================================================*/
static int __init stage12_soft_init(void)
{
    struct net_device *ndev;
    struct stage12_priv *priv;
    int i, ret;

    num_queues = clamp_t(unsigned int, num_queues, 1, STAGE12_MAX_QUEUES);
    ring_size = max_t(unsigned int, ring_size, 32);
    napi_weight = max_t(unsigned int, napi_weight, 16);
    backend_batch = max_t(unsigned int, backend_batch, 1);

    ndev = alloc_etherdev_mqs(sizeof(struct stage12_priv), num_queues, num_queues);
    if (!ndev)
        return -ENOMEM;

    strscpy(ndev->name, ifname, IFNAMSIZ);
    ndev->netdev_ops = &stage12_netdev_ops;
    ndev->ethtool_ops = &stage12_ethtool_ops;
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

    priv->backend_wq = alloc_workqueue("stage12s_backend", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!priv->backend_wq) {
        free_netdev(ndev);
        return -ENOMEM;
    }
    priv->irq_wq = alloc_workqueue("stage12s_irq", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!priv->irq_wq) {
        destroy_workqueue(priv->backend_wq);
        free_netdev(ndev);
        return -ENOMEM;
    }

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage12_queue *q = &priv->queues[i];
        q->priv = priv;
        q->qid = i;
        q->vector_id = i;
        INIT_WORK(&q->backend_work, stage12_backend_workfn);
        INIT_WORK(&q->irq_work, stage12_irq_workfn);
        ret = stage12_alloc_ring(&q->txq, ring_size);
        if (ret)
            goto err;
        ret = stage12_alloc_ring(&q->rxq, ring_size);
        if (ret)
            goto err;
        STAGE12_NETIF_NAPI_ADD(ndev, &q->napi, stage12_napi_poll, napi_weight);

        /* 每队列独立 page_pool */
        q->pp = stage12_create_page_pool(q);
        if (IS_ERR_OR_NULL(q->pp)) {
            ret = PTR_ERR(q->pp);
            q->pp = NULL;
            goto err;
        }

        /* vector 初始化 */
        priv->vectors[i].vector_id = i;
        priv->vectors[i].qid = i;
        priv->vectors[i].target_cpu = stage12_pick_irq_cpu(i);
        snprintf(priv->vectors[i].name, sizeof(priv->vectors[i].name), "stage12s-q%u", i);
        priv->vectors[i].last_raise_cpu = -1;
        priv->vectors[i].last_handle_cpu = -1;

        stage12_reset_queue(q);
    }

    ret = register_netdev(ndev);
    if (ret)
        goto err;

    stage12_debugfs_init(priv);
    stage12_soft_ndev = ndev;
    pr_info("%s: loaded ifname=%s num_queues=%u ring_size=%u napi_weight=%u backend_batch=%u\n",
            DRV_NAME, ndev->name, priv->num_queues, priv->ring_size,
            priv->napi_weight, priv->backend_batch);
    return 0;
err:
    for (i = 0; i < priv->num_queues; ++i) {
        if (priv->queues[i].napi.dev)
            netif_napi_del(&priv->queues[i].napi);
        stage12_free_ring(ndev, &priv->queues[i].txq, false);
        stage12_free_ring(ndev, &priv->queues[i].rxq, true);
        stage12_destroy_page_pool(&priv->queues[i]);
    }
    if (priv->irq_wq)
        destroy_workqueue(priv->irq_wq);
    destroy_workqueue(priv->backend_wq);
    free_netdev(ndev);
    return ret;
}

static void __exit stage12_soft_exit(void)
{
    struct stage12_priv *priv;
    int i;

    if (!stage12_soft_ndev)
        return;
    priv = netdev_priv(stage12_soft_ndev);
    stage12_debugfs_deinit(priv);
    unregister_netdev(stage12_soft_ndev);
    flush_workqueue(priv->backend_wq);
    flush_workqueue(priv->irq_wq);
    destroy_workqueue(priv->backend_wq);
    destroy_workqueue(priv->irq_wq);
    for (i = 0; i < priv->num_queues; ++i) {
        cancel_work_sync(&priv->queues[i].backend_work);
        cancel_work_sync(&priv->queues[i].irq_work);
        netif_napi_del(&priv->queues[i].napi);
        stage12_free_ring(stage12_soft_ndev, &priv->queues[i].txq, false);
        stage12_free_ring(stage12_soft_ndev, &priv->queues[i].rxq, true);
        stage12_destroy_page_pool(&priv->queues[i]);
    }
    free_netdev(stage12_soft_ndev);
    stage12_soft_ndev = NULL;
    pr_info("%s: unloaded\n", DRV_NAME);
}

module_init(stage12_soft_init);
module_exit(stage12_soft_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Lab");
MODULE_DESCRIPTION("stage12 soft page_pool netdev");