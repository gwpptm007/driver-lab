// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage14_soft.c — stage14 纯软教学模型 + XDP
 *
 * 在 stage13 基础上增加 XDP 支持：
 *   1. ndo_xdp 回调注册
 *   2. xdp_buff 处理路径（在 build_skb 之前）
 *   3. XDP_PASS / XDP_DROP / XDP_TX / XDP_REDIRECT action
 *   4. XDP 统计 (xdp_pass, xdp_drop, xdp_tx, xdp_redirect)
 *   5. BPF program 通过 ip link 加载
 *
 * 上下文：Linux kernel netdev子系统 + XDP
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
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/bpf.h>
#include <linux/filter.h>
#include <linux/rcupdate.h>
#include <net/xdp.h>

#include "../include/netdev_stage14_compat.h"

#define DRV_NAME    "netdev_stage14_soft"
#define STAGE14_MAX_QUEUES      4
#define STAGE14_DEFAULT_NUM_QUEUES  2
#define STAGE14_DEFAULT_RING_SIZE  128
#define STAGE14_DEFAULT_NAPI_WEIGHT 64
#define STAGE14_DEFAULT_BACKEND_BATCH 64
#define STAGE14_DEFAULT_BACKEND_DELAY_US 0
#define STAGE14_QUEUE_DUMP_LIMIT 8

#define STAGE14_TEST_PROTO  0x0800
#define STAGE14_TEST_MAGIC  "STAGE14"

/*========================================================
 *     底层数据结构
 *========================================================*/

/* slot 状态机 — 5 状态
 *
 * TX path: FREE → SUBMITTED → DONE → FREE
 * RX path: FREE → POSTED → READY → DONE → FREE
 */
enum stage14_slot_state {
    S14_SLOT_FREE = 0,
    S14_SLOT_POSTED,   /* RX: page 已分配，等待 backend 填充 */
    S14_SLOT_SUBMITTED, /* TX: skb 已提交，等待 backend 处理 */
    S14_SLOT_READY,    /* RX: backend 已填充数据，等待 napi 消费 */
    S14_SLOT_DONE,     /* TX: 传输完成 / RX: 已消费，等待 refill */
};

/* 描述符（存放元数据，不存放数据） */
struct stage14_desc {
    u32 data_len;
    u16 state;
    u16 flags;
};

/* Buffer slot — 使用 page（来自 page_pool） */
struct stage14_buf_slot {
    struct page *page;      /* 来自 page_pool 的 page */
    void *buf;               /* page_address(page)，用于 bounce copy */
    u16 buf_len;
    u16 data_len;
    enum stage14_slot_state state;
    u16 id;
    u32 last_seq;
};

/* TX/RX ring */
struct stage14_ring {
    struct stage14_desc  *desc;
    struct stage14_buf_slot *slots;
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
struct stage14_timeline {
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
struct stage14_vector_stats {
    atomic64_t raise_count;
    atomic64_t handle_count;
    atomic64_t schedule_count;
};

struct stage14_vector {
    u16 vector_id;
    u16 qid;
    int target_cpu;
    char name[32];
    struct stage14_vector_stats stats;
    u64 last_raise_ns;
    u64 last_handle_ns;
    int last_raise_cpu;
    int last_handle_cpu;
};

/* XDP 统计 */
struct stage14_xdp_stats {
    atomic64_t xdp_pass;
    atomic64_t xdp_drop;
    atomic64_t xdp_tx;
    atomic64_t xdp_redirect;
    atomic64_t xdp_err;
};

/* Per-queue 统计 */
struct stage14_queue_stats {
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
    atomic64_t tx_csum_partial_count;
    atomic64_t tx_gso_packets;
    atomic64_t rx_gro_packets;
    atomic64_t feature_set_count;

    /* XDP 统计 */
    struct stage14_xdp_stats xdp;
};

/* Per-queue 上下文 */
struct stage14_queue {
    struct stage14_priv *priv;
    u16 qid;

    /* NAPI */
    struct napi_struct napi;

    /* XDP RXQ info（bpf_prog_run_xdp 需要） */
    struct xdp_rxq_info xdp_rxq;

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
    struct stage14_ring txq;
    struct stage14_ring rxq;

    /* timeline */
    struct stage14_timeline timeline;

    /* 统计 */
    struct stage14_queue_stats stats;

    /* 每队列独立 page_pool（核心设计，compat 层） */
    struct stage14_page_pool *pp;
};

/* priv */
struct stage14_priv {
    struct net_device *ndev;
    spinlock_t state_lock;

    /* backend workqueue */
    struct workqueue_struct *backend_wq;

    /* irq workqueue（用于 vector 中断模拟） */
    struct workqueue_struct *irq_wq;

    /* debugfs */
    struct dentry *dbg_dir;

    /* XDP */
    struct bpf_prog __rcu *xdp_prog;

    /* 参数 */
    u32 num_queues;
    u32 ring_size;
    u32 napi_weight;
    u32 rx_buf_size;
    u32 backend_delay_us;
    u32 backend_batch;
    u32 ethtool_priv_flags;
    netdev_features_t last_features;

    atomic64_t rr_counter;
    atomic64_t open_count;
    atomic64_t stop_count;
    atomic64_t xdp_prog_set_count;
    atomic64_t xdp_prog_clear_count;

    /* per-queue vectors */
    struct stage14_vector vectors[STAGE14_MAX_QUEUES];

    /* per-queue 上下文 */
    struct stage14_queue queues[STAGE14_MAX_QUEUES];
};

/*========================================================
 *     模块参数
 *========================================================*/
static char ifname[IFNAMSIZ] = "nds14s";
module_param_string(ifname, ifname, sizeof(ifname), 0444);
MODULE_PARM_DESC(ifname, "network interface name");

static unsigned int num_queues = STAGE14_DEFAULT_NUM_QUEUES;
module_param(num_queues, uint, 0444);
MODULE_PARM_DESC(num_queues, "number of TX/RX queue pairs");

static unsigned int ring_size = STAGE14_DEFAULT_RING_SIZE;
module_param(ring_size, uint, 0444);
MODULE_PARM_DESC(ring_size, "descriptor ring size");

static unsigned int napi_weight = STAGE14_DEFAULT_NAPI_WEIGHT;
module_param(napi_weight, uint, 0444);
MODULE_PARM_DESC(napi_weight, "NAPI poll budget");

static unsigned int backend_delay_us = STAGE14_DEFAULT_BACKEND_DELAY_US;
module_param(backend_delay_us, uint, 0644);
MODULE_PARM_DESC(backend_delay_us, "simulated backend delay in microseconds");

static unsigned int backend_batch = STAGE14_DEFAULT_BACKEND_BATCH;
module_param(backend_batch, uint, 0444);
MODULE_PARM_DESC(backend_batch, "backend batch size");

static unsigned int rx_buf_size = 2048;
module_param(rx_buf_size, uint, 0444);
MODULE_PARM_DESC(rx_buf_size, "RX buffer size");

/* 全局 ndev 指针 */
static struct net_device *stage14_soft_ndev;

/*========================================================
 *     时间戳
 *========================================================*/
static inline u64 stage14_now_ns(void)
{
    return ktime_get_ns();
}

/*========================================================
 *     工具函数
 *========================================================*/
static inline u16 stage14_next_idx(u16 idx, u16 size)
{
    return (idx + 1) % size;
}

/* 选择 vector 对应的 CPU（round-robin） */
static int stage14_pick_irq_cpu(u16 qid)
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

static struct stage14_vector *stage14_get_vector(struct stage14_priv *priv, u16 qid)
{
    if (qid >= priv->num_queues)
        return NULL;
    return &priv->vectors[qid];
}

static inline bool stage14_is_test_frame(struct sk_buff *skb)
{
    const unsigned char *base;
    const struct iphdr *iph;
    const unsigned char *payload;

    if (!skb || skb_headlen(skb) < sizeof(struct iphdr) + sizeof(struct udphdr) + 8)
        return false;

    base = skb->data;
    if ((base[0] >> 4) == 4) {
        iph = (const struct iphdr *)base;
    } else {
        if (skb_headlen(skb) < ETH_HLEN + sizeof(struct iphdr) + sizeof(struct udphdr) + 8)
            return false;
        iph = (const struct iphdr *)(base + ETH_HLEN);
        if ((iph->version) != 4)
            return false;
    }

    if (iph->protocol != IPPROTO_UDP)
        return false;
    payload = (const unsigned char *)iph + iph->ihl * 4 + sizeof(struct udphdr);
    return !memcmp(payload, STAGE14_TEST_MAGIC, sizeof(STAGE14_TEST_MAGIC) - 1);
}

/*========================================================
 *     page_pool 兼容层（每队列独立）
 *
 *     为什么每队列独立 page_pool？
 *       - 真实驱动每个 RX 队列对应一个独立的 HW DMA 环
 *       - 每队列独立 pool 可以避免跨队列的 cache line 竞争
 *       - page_pool 支持按队列统计（rx_page_alloc 等）
 *
 *     软模型 fallback：
 *       - 使用 alloc_pages(GFP_ATOMIC | __GFP_COMP, 0) 替代 page_pool_dev_alloc_pages
 *       - 使用 put_page() 替代 page_pool_put_page
 *       - 不需要真实的 page_pool_destroy（fallback 只做 kfree）
 *========================================================*/
static struct stage14_page_pool *stage14_create_page_pool(struct stage14_queue *q)
{
    struct stage14_page_pool *pp;

    pp = kzalloc(sizeof(*pp), GFP_KERNEL);
    return pp;
}

static void stage14_destroy_page_pool(struct stage14_queue *q)
{
    if (q->pp) {
        kfree(q->pp);
        q->pp = NULL;
    }
}

/*========================================================
 *     XDP RXQ 初始化 / 销毁
 *
 *     xdp_rxq_info 是 XDP 处理的核心元数据：
 *       - 告诉 bpf_prog_run_xdp() 这个 buffer 来自哪个 netdev 和 queue
 *       - 注册内存模型（MEN_TYPE_PAGE_ORDER0 = 全页，无 DMA 映射）
 *
 *     调用时机：page_pool 创建之后、NAPI 注册之后
 *     销毁时机：module exit 的 napi_disable 之后
 *========================================================*/
static int stage14_xdp_rxq_init(struct stage14_queue *q)
{
    struct net_device *ndev = q->priv->ndev;
    int err;

    /* xdp_rxq_info_reg needs a valid napi_id - at this point napi is registered */
    err = xdp_rxq_info_reg(&q->xdp_rxq, ndev, q->qid, q->napi.napi_id);
    if (err)
        return err;
    /* 使用 MEM_TYPE_PAGE_ORDER0（每 slot 全页，无 page_pool 依赖） */
    err = xdp_rxq_info_reg_mem_model(&q->xdp_rxq, MEM_TYPE_PAGE_ORDER0, NULL);
    if (err)
        xdp_rxq_info_unreg(&q->xdp_rxq);
    return err;
}

static void stage14_xdp_rxq_deinit(struct stage14_queue *q)
{
    xdp_rxq_info_unreg(&q->xdp_rxq);
}

/*========================================================
 *     内存分配 / 释放
 *========================================================*/
static int stage14_alloc_ring(struct stage14_ring *r, u16 size)
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

static void stage14_free_ring(struct net_device *ndev, struct stage14_ring *r, struct stage14_page_pool *pp, bool is_rx)
{
    u16 i;
    if (!r->slots)
        goto out;
    for (i = 0; i < r->size; ++i) {
        struct stage14_buf_slot *s = &r->slots[i];
        if (s->page) {
            /* RX: 槽中的 page 可能在 napi_poll 路径中被消费并通过
             * build_skb destructor 异步释放。此处不调用 put_page，
             * 只清除 slot 指针。page_pool_destroy 会处理泄漏的页。
             * napi_disable 已确保此时无 napi_poll 正在访问 slot。 */
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

static void stage14_reset_queue(struct stage14_queue *q)
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
 *
 *     refill 流程（RX slot 消费完成后）：
 *       1. 从 page_pool 分配一个新 page
 *       2. 填充到被消费的空 slot
 *       3. 状态设为 POSTED
 *       4. 更新 post_idx（生产端索引）
 *
 *     注意：
 *       - page 被 build_skb 消费后不显式回收（依赖 skb destructor）
 *       - 只有 build_skb 失败路径才需要显式 recycle
 *       - refill 是保持 RX 环持续可用的关键
 *========================================================*/

/* 为 RX slot 补充一个 page（从 page_pool 分配） */
static int stage14_refill_rx_slot(struct stage14_queue *q, u16 idx)
{
    struct stage14_priv *priv = q->priv;
    struct stage14_buf_slot *slot = &q->rxq.slots[idx];
    struct page *page;

    if (slot->page)
        return 0;

    page = stage14_page_pool_alloc(q->pp);
    if (!page)
        return -ENOMEM;

    slot->page = page;
    slot->buf = page_address(page);
    slot->buf_len = priv->rx_buf_size;
    slot->data_len = 0;
    slot->last_seq = 0;
    slot->state = S14_SLOT_POSTED;
    q->rx_posted++;
    atomic64_inc(&q->stats.pp_alloc);
    atomic64_inc(&q->stats.rx_post_count);
    return 0;
}

/*========================================================
 *     MSI-X 语义模拟：irq_workfn
 *
 *     软模型模拟真实 NIC 的 MSI-X 中断处理：
 *       1. backend_work 完成后调用 stage14_raise_irq()
 *       2. raise_irq() 将 irq_work 排队到 irq_wq（目标 CPU）
 *       3. irq_workfn 执行：napi_schedule_prep() + __napi_schedule()
 *          → 将 NAPI poll 加入调度，下一次 softirq 执行时处理
 *
 *     注意：真实硬件中 MSI 中断直接注入 CPU，NAPI 通过 hardirq 回调触发
 *           软模型用 workqueue 模拟这个延迟异步行为
 *========================================================*/
static void stage14_irq_workfn(struct work_struct *work)
{
    struct stage14_queue *q = container_of(work, struct stage14_queue, irq_work);
    struct stage14_priv *priv = q->priv;
    struct stage14_vector *vec = stage14_get_vector(priv, q->qid);
    unsigned long flags;

    spin_lock_irqsave(&priv->state_lock, flags);
    q->timeline.last_irq_ns = stage14_now_ns();
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
static void stage14_raise_irq(struct stage14_queue *q)
{
    struct stage14_priv *priv = q->priv;
    struct stage14_vector *vec = stage14_get_vector(priv, q->qid);

    if (vec) {
        atomic64_inc(&vec->stats.raise_count);
        atomic64_inc(&vec->stats.schedule_count);
        vec->last_raise_ns = stage14_now_ns();
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
 *
 *     doorbell 是真实 NIC 的写操作（通常是 MMIO 写）：
 *       - 驱动写一个特定地址 → NIC 感知到"有新 TX 包要处理"
 *       - NIC 消费 TX 包 → 产生 RX 包 → 触发 MSI 中断
 *
 *     软模型模拟：
 *       - doorbell_pending 标志防止重复调度
 *       - queue_work() 将 backend_work 排队到 workqueue
 *       - backend_work 同步执行（不等待，异步）
 *
 *     doorbell_pending 作用：
 *       - 防止同一时刻有多个 backend_work 在队列中
 *       - 如果上一个 work 还没执行完，下一个 doorbell 只设置标志位
 *       - 上一个 work 完成后检查标志位，决定是否需要继续调度
 *========================================================*/
static void stage14_mark_doorbell(struct stage14_queue *q)
{
    q->timeline.last_doorbell_ns = stage14_now_ns();
    atomic64_inc(&q->stats.doorbell_count);
    atomic64_inc(&q->stats.backend_schedule_count);
    if (!q->doorbell_pending) {
        q->doorbell_pending = true;
        queue_work(q->priv->backend_wq, &q->backend_work);
    }
}

/*========================================================
 *     TX complete（在 NAPI poll 中）
 *
 *     消费 TX ring 中状态为 DONE 的 slot：
 *       - 释放 kmalloc 的 bounce buffer（kfree）
 *       - 清除 slot 状态为 FREE
 *       - 更新 complete_idx 索引
 *       - 减少 tx_inflight / tx_done 计数
 *
 *     注意：bounce buffer 的生命周期：
 *       start_xmit() 分配 (kmalloc) → backend_work 标记 DONE
 *                                    → napi_poll 调用 complete_tx_one 释放 (kfree)
 *========================================================*/
static void stage14_complete_tx_one(struct stage14_queue *q)
{
    struct stage14_ring *r = &q->txq;
    struct stage14_desc *d = &r->desc[r->complete_idx];
    struct stage14_buf_slot *s = &r->slots[r->complete_idx];

    if (!q->tx_done || s->state != S14_SLOT_DONE)
        return;

    /* soft model: TX slot 使用 bounce buffer（kmalloc） */
    kfree(s->buf);
    s->buf = NULL;
    s->page = NULL;
    memset(s, 0, sizeof(*s));
    memset(d, 0, sizeof(*d));
    r->complete_idx = stage14_next_idx(r->complete_idx, r->size);
    q->tx_done--;
    q->tx_inflight--;
    q->timeline.last_complete_ns = stage14_now_ns();
    atomic64_inc(&q->stats.tx_complete_count);
}

/*========================================================
 *     XDP 处理路径（在 build_skb 之前）
 *
 *     XDP 是数据包最早的处理点，在 page 层面直接处理。
 *     不创建 skb，直接操作 xdp_buff。
 *
 *     xdp_buff 结构：
 *       - data: 指向数据包起始
 *       - data_end: 指向数据包结束
 *       - data_meta: 元数据区（可用于传递额外信息）
 *       - rxq: 指向 xdp_rxq_info（包含 dev/queue/mem_type）
 *
 *     RCU 保护：
 *       - ndo_bpf 可能与 NAPI poll 并发执行
 *       - 使用 rcu_dereference() 安全读取 priv->xdp_prog
 *
 *     返回值语义：
 *       - XDP_PASS: 继续走 build_skb 路径，上送协议栈
 *       - XDP_DROP: page 归还 pool，不上送协议栈
 *       - XDP_TX: 软模型仅消费 page（无真正 DMA TX 路径）
 *       - XDP_REDIRECT: 软模型仅消费 page（无真正 redirect 路径）
 *       - default: 记 xdp_err，转 XDP_PASS
 *========================================================*/
static int stage14_xdp_process(struct stage14_queue *q, struct stage14_buf_slot *s,
                               void *buf, u32 len)
{
    struct stage14_priv *priv = q->priv;
    struct bpf_prog *prog;
    struct xdp_buff xdp;
    u32 act;

    /* RCU protected read of xdp_prog */
    prog = rcu_dereference(priv->xdp_prog);
    if (!prog)
        return XDP_PASS;

    xdp_init_buff(&xdp, priv->rx_buf_size, &q->xdp_rxq);
    xdp_prepare_buff(&xdp, buf, 0, len, false);

    act = bpf_prog_run_xdp(prog, &xdp);

    switch (act) {
    case XDP_PASS:
        /* XDP_PASS: 继续走 build_skb 路径上送协议栈 */
        atomic64_inc(&q->stats.xdp.xdp_pass);
        return XDP_PASS;
    case XDP_DROP:
        /* XDP_DROP: page 归还 pool，不上送协议栈 */
        atomic64_inc(&q->stats.xdp.xdp_drop);
        return XDP_DROP;
    case XDP_TX:
        /* XDP_TX: 软模型无真正 DMA TX 路径，page 消费后丢弃 */
        atomic64_inc(&q->stats.xdp.xdp_tx);
        return XDP_TX;
    case XDP_REDIRECT:
        /* XDP_REDIRECT: 软模型无真正 redirect，page 消费后丢弃 */
        atomic64_inc(&q->stats.xdp.xdp_redirect);
        return XDP_REDIRECT;
    default:
        atomic64_inc(&q->stats.xdp.xdp_err);
        return XDP_PASS;
    }
}

/*========================================================
 *     RX consume（在 NAPI poll 中）— XDP + build_skb 零拷贝版本
 *
 *     流程：
 *       1. 检查是否有 XDP program 注册
 *       2. 如果有：走 XDP 处理路径
 *       3. 如果无：走 stage13 的 build_skb 路径
 *
 *     XDP 路径（stage14 新增）：
 *       - 调用 stage14_xdp_process() 获取 action
 *       - XDP_PASS: 继续走 build_skb（packet 上送协议栈）
 *       - XDP_DROP / TX / REDIRECT:
 *           page 归还 page_pool，refill slot，不上送协议栈
 *         （TX/REDIRECT 在软模型中仅"消费 page"，无真正发送能力）
 *
 *     build_skb 路径（与 stage13 相同）：
 *       - 成功：skb destructor 的 put_page 自动归 page 回 pool
 *       - 失败：stage14_page_pool_recycle 显式回收
 *
 *     GRO 路径：
 *       - GRO enabled: napi_gro_receive() 合并后再上送
 *       - GRO disabled: netif_receive_skb() 逐包上送
 *========================================================*/
static int stage14_consume_rx_one(struct stage14_queue *q)
{
    struct stage14_priv *priv = q->priv;
    struct net_device *ndev = priv->ndev;
    struct stage14_ring *r = &q->rxq;
    struct stage14_desc *d = &r->desc[r->consume_idx];
    struct stage14_buf_slot *s = &r->slots[r->consume_idx];
    struct page *page = s->page;
    void *buf = s->buf;
    u32 len = s->data_len;
    struct sk_buff *skb;
    u16 idx = r->consume_idx;

    if (!q->rx_ready || s->state != S14_SLOT_READY || !page)
        return 0;

    /* XDP 处理路径：在 build_skb 之前先检查 XDP */
    if (rcu_dereference(priv->xdp_prog)) {
        int xdp_act = stage14_xdp_process(q, s, buf, len);

        /* XDP_PASS: 继续走 build_skb 上送协议栈
         * XDP_DROP / TX / REDIRECT: page 消费后丢弃，不上送协议栈
         *   - XDP_TX: 软模型无真正 DMA TX，只消费 page
         *   - XDP_REDIRECT: 软模型无真正 redirect，只消费 page
         *   （真实驱动 XDP_TX/REDIRECT 有硬件路径） */
        if (xdp_act != XDP_PASS) {
            /* 归还 page 并 refill，保持 ring 持续可用 */
            stage14_page_pool_put(q->pp, page);
            memset(s, 0, sizeof(*s));
            memset(d, 0, sizeof(*d));
            r->consume_idx = stage14_next_idx(r->consume_idx, r->size);
            q->rx_ready--;
            stage14_refill_rx_slot(q, idx);
            return 0;
        }
        /* XDP_PASS: 继续走 build_skb 路径 */
    }

    /* build_skb 路径：从 page 构建 skb */
    skb = build_skb(buf, priv->rx_buf_size);
    if (!skb) {
        /* build_skb 失败：page 未被使用，通过 page_pool 归还 */
        stage14_page_pool_recycle(q->pp, page);
        atomic64_inc(&q->stats.pp_build_skb_fail);
        memset(s, 0, sizeof(*s));
        memset(d, 0, sizeof(*d));
        r->consume_idx = stage14_next_idx(r->consume_idx, r->size);
        q->rx_ready--;
        stage14_refill_rx_slot(q, idx);
        return 0;
    }

    skb->dev = ndev;
    skb_put(skb, len);
    skb->protocol = eth_type_trans(skb, ndev);
    if (stage14_is_test_frame(skb))
        atomic64_inc(&q->stats.test_rx_consume_count);

    if (ndev->features & NETIF_F_GRO) {
        napi_gro_receive(&q->napi, skb);
        atomic64_inc(&q->stats.rx_gro_packets);
    } else {
        netif_receive_skb(skb);
    }

    memset(s, 0, sizeof(*s));
    memset(d, 0, sizeof(*d));
    r->consume_idx = stage14_next_idx(r->consume_idx, r->size);
    q->rx_ready--;
    q->timeline.last_consume_ns = stage14_now_ns();
    atomic64_inc(&q->stats.rx_consume_count);
    atomic64_inc(&q->stats.rx_packets);
    atomic64_add(len, &q->stats.rx_bytes);

    stage14_refill_rx_slot(q, idx);
    return 1;
}

/*========================================================
 *     backend workfn — 异步处理 TX/RX
 *
 *     软模型中的"backend"模拟 NIC 硬件行为：
 *       - 从 TX ring 取走已提交的 skb（COPY 到 RX side）
 *       - 填充 RX slot（相当于 NIC DMA 填充 RX buffer）
 *       - 完成后触发 IRQ，通知 NAPI 收割
 *
 *     数据流（软模型回环）：
 *       TX slot[SUBMITTED] → backend_copy → RX slot[READY] → IRQ → NAPI poll
 *
 *     注意：真实硬件中 TX 和 RX 是独立的 DMA 通道，
 *           软模型用 copy 模拟硬件的"接收外部数据包"行为
 *========================================================*/
static void stage14_backend_workfn(struct work_struct *work)
{
    struct stage14_queue *q = container_of(work, struct stage14_queue, backend_work);
    struct stage14_priv *priv = q->priv;
    unsigned long flags;
    int processed = 0;
    bool need_resched = false;

    if (priv->backend_delay_us)
        usleep_range(priv->backend_delay_us, priv->backend_delay_us + 50);

    spin_lock_irqsave(&priv->state_lock, flags);
    q->doorbell_pending = false;
    q->backend_running = true;
    q->timeline.last_backend_wakeup_ns = stage14_now_ns();
    atomic64_inc(&q->stats.backend_run_count);

    while (processed < priv->backend_batch) {
        struct stage14_ring *txr = &q->txq;
        struct stage14_ring *rxr = &q->rxq;
        struct stage14_desc *txd, *rxd;
        struct stage14_buf_slot *txs, *rxs;
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

        if (txs->state != S14_SLOT_SUBMITTED)
            break;
        if (rxs->state != S14_SLOT_POSTED)
            break;

        /* 模拟 backend 收到 TX 数据并转发到 RX */
        copy_len = min(txd->data_len, rxs->buf_len);
        memcpy(rxs->buf, txs->buf, copy_len);
        rxs->data_len = copy_len;
        rxs->last_seq = txs->last_seq;
        rxs->state = S14_SLOT_READY;   /* 状态变为 READY */
        rxd->data_len = copy_len;
        rxd->state = S14_SLOT_READY;
        rxr->device_idx = stage14_next_idx(rxr->device_idx, rxr->size);
        q->rx_posted--;
        q->rx_ready++;
        atomic64_inc(&q->stats.rx_ready_count);

        txs->state = S14_SLOT_DONE;
        txd->state = S14_SLOT_DONE;
        txr->notify_idx = stage14_next_idx(txr->notify_idx, txr->size);
        q->tx_done++;

        atomic64_inc(&q->stats.backend_tx_processed);
        atomic64_inc(&q->stats.backend_rx_produced);
        processed++;
    }

    q->timeline.last_backend_done_ns = stage14_now_ns();
    /* 触发 MSI-X 语义模拟 */
    if (processed)
        stage14_raise_irq(q);
    if (q->txq.notify_idx != q->txq.submit_idx)
        need_resched = true;
    q->backend_running = false;
    spin_unlock_irqrestore(&priv->state_lock, flags);

    if (need_resched)
        stage14_mark_doorbell(q);
}

/*========================================================
 *     NAPI poll — 消费 TX done 和 RX ready
 *
 *     NAPI 是 Linux 网络栈的批处理轮询机制（替代频繁 IRQ）：
 *       1. IRQ 触发 → napi_schedule() → 进入 poll 列表
 *       2. poll() 被调用 → 按 budget 批量处理（最多处理 budget 个包）
 *       3. 处理完成后 napi_complete() → 重新使能 IRQ
 *
 *     本驱动处理顺序：
 *       - 先完成 TX（回收 TX slot，释放 skb）
 *       - 再消费 RX（走 XDP 或 build_skb 路径）
 *       - 最后检查是否需要继续调度（doorbell pending）
 *========================================================*/
static int stage14_napi_poll(struct napi_struct *napi, int budget)
{
    struct stage14_queue *q = container_of(napi, struct stage14_queue, napi);
    struct stage14_priv *priv = q->priv;
    unsigned long flags;
    int work = 0;

    spin_lock_irqsave(&priv->state_lock, flags);
    q->timeline.last_poll_ns = stage14_now_ns();
    atomic64_inc(&q->stats.napi_poll_count);

    /* TX complete */
    while (q->tx_done)
        stage14_complete_tx_one(q);

    /* RX consume */
    while (q->rx_ready && work < budget)
        work += stage14_consume_rx_one(q);

    atomic64_add(work, &q->stats.napi_work_total);
    if (!q->rx_ready && !q->tx_done) {
        STAGE14_NAPI_COMPLETE(napi, work);
        q->irq_masked = false;
        atomic64_inc(&q->stats.napi_complete_count);
        if (q->doorbell_pending || q->txq.notify_idx != q->txq.submit_idx)
            stage14_mark_doorbell(q);
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);
    return work;
}

/*========================================================
 *     ndo_start_xmit — 数据包发送入口
 *
 *     调用路径：netif_tx → dev_queue_xmit → ndo_start_xmit
 *
 *     流程：
 *       1. skb_get_queue_mapping() — 根据 hash 选择 TX 队列
 *       2. 检查 ring 是否有空闲 slot
 *       3. skb_linearize() — 非线性 skb 线性化（软模型要求）
 *       4. bounce buffer copy — 软模型用 kmalloc 模拟 DMA 复制
 *       5. 提交到 TX ring (SUBMITTED) → 标记 doorbell → 触发 backend work
 *
 *     返回值：
 *       - NETDEV_TX_OK: 包已入 ring，等 doorbell 通知
 *       - NETDEV_TX_BUSY: ring 满，需上层协议栈重试
 *========================================================*/
static netdev_tx_t stage14_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    struct stage14_priv *priv = netdev_priv(ndev);
    struct stage14_queue *q;
    struct stage14_ring *r;
    struct stage14_desc *d;
    struct stage14_buf_slot *s;
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
    if (s->state != S14_SLOT_FREE) {
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
    s->state = S14_SLOT_SUBMITTED;
    s->id = idx;
    d->data_len = skb_headlen(skb);
    d->state = S14_SLOT_SUBMITTED;
    r->submit_idx = stage14_next_idx(r->submit_idx, r->size);
    q->tx_inflight++;

    q->timeline.last_submit_ns = stage14_now_ns();
    atomic64_inc(&q->stats.tx_submit_count);
    atomic64_inc(&q->stats.tx_packets);
    atomic64_add(skb_headlen(skb), &q->stats.tx_bytes);
    if (stage14_is_test_frame(skb))
        atomic64_inc(&q->stats.test_tx_submit_count);
    if (skb->ip_summed == CHECKSUM_PARTIAL && (ndev->features & NETIF_F_HW_CSUM))
        atomic64_inc(&q->stats.tx_csum_partial_count);
    if (skb_is_gso(skb) && (ndev->features & NETIF_F_GSO_SOFTWARE))
        atomic64_inc(&q->stats.tx_gso_packets);

    stage14_mark_doorbell(q);
    spin_unlock_irqrestore(&priv->state_lock, flags);
    return NETDEV_TX_OK;
}

/*========================================================
 *     ndo_select_queue
 *========================================================*/
static u16 stage14_select_queue(struct net_device *ndev, struct sk_buff *skb,
                                 struct net_device *sb_dev)
{
    struct stage14_priv *priv = netdev_priv(ndev);
    u32 hash = skb_get_hash(skb);

    if (hash)
        return reciprocal_scale(hash, priv->num_queues);
    return atomic64_inc_return(&priv->rr_counter) % priv->num_queues;
}

/*========================================================
 *     ndo_open / ndo_stop — 接口开关
 *
 *     stage14_open():
 *       1. 重置所有 RX queue 状态
 *       2. 预填充所有 RX slot（从 page_pool 分配 page）
 *       3. napi_enable() — 使能 NAPI 轮询
 *       4. netif_tx_start_all_queues() — 启动 TX
 *
 *     stage14_stop():
 *       1. netif_tx_disable() — 停止 TX（防止新包入）
 *       2. flush_workqueue() — 等待所有 backend work 完成
 *       3. napi_disable() — 禁用 NAPI（等待 poll 完全退出）
 *       注意：napi_disable 必须在 unregister_netdev 之前调用，
 *             否则 unregister_netdev 内部也会调 napi_disable，形成嵌套等待
 *========================================================*/
static int stage14_open(struct net_device *ndev)
{
    struct stage14_priv *priv = netdev_priv(ndev);
    unsigned long flags;
    int i;

    spin_lock_irqsave(&priv->state_lock, flags);
    for (i = 0; i < priv->num_queues; ++i) {
        stage14_reset_queue(&priv->queues[i]);
        /* 预填充所有 RX slot */
        while (priv->queues[i].rx_posted < priv->queues[i].rxq.size - 1) {
            if (stage14_refill_rx_slot(&priv->queues[i],
                                        priv->queues[i].rxq.post_idx) != 0)
                break;
            priv->queues[i].rxq.post_idx = stage14_next_idx(
                priv->queues[i].rxq.post_idx, priv->queues[i].rxq.size);
        }
        napi_enable(&priv->queues[i].napi);
    }
    atomic64_inc(&priv->open_count);
    spin_unlock_irqrestore(&priv->state_lock, flags);

    netif_tx_start_all_queues(ndev);
    return 0;
}

static int stage14_stop(struct net_device *ndev)
{
    struct stage14_priv *priv = netdev_priv(ndev);
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
 *     ndo_bpf — XDP program 注册 / 替换 / 卸载
 *
 *     内核调用路径：
 *       ip link set dev nds14s xdp obj xdp.o
 *           → rtnetlink_rcv_msg()
 *           → dev->netdev_ops->ndo_bpf(dev, bpf)
 *
 *     XDP_SETUP_PROG（加载或替换，prog != NULL）：
 *       - bpf_prog_inc(prog) — 新 prog 引用 +1
 *       - rcu_dereference(old) — 取出旧 prog（如果有）
 *       - rcu_assign_pointer(new) — 写入新 prog
 *       - synchronize_net() + bpf_prog_put(old) — 释放旧 prog（如果有）
 *       - xdp_prog_set_count++ — 统计
 *       注意：必须先取出旧 prog 再写入新 prog，否则旧 prog 引用泄漏
 *
 *     XDP_SETUP_PROG（卸载，prog=NULL）：
 *       - rcu_dereference() — 读取旧 prog
 *       - rcu_assign_pointer(, NULL) — 清空指针
 *       - synchronize_net() — 等待所有 RCU 读者（NAPI poll）完成
 *       - bpf_prog_put() — 释放 prog
 *       - xdp_prog_clear_count++ — 统计
 *
 *     RCU 保护机制说明：
 *       - RCU（Read-Copy-Update）是 Linux 内核的并发读取同步机制
 *       - 读者：NAPI poll 中 rcu_dereference() 读取 priv->xdp_prog
 *       - 写者：先写入新指针，再调用 synchronize_net() 等待所有旧指针读者退出，再释放旧对象
 *========================================================*/
static int stage14_xdp(struct net_device *ndev, struct netdev_bpf *bpf)
{
    struct stage14_priv *priv = netdev_priv(ndev);
    struct bpf_prog *prog, *old;

    switch (bpf->command) {
    case XDP_SETUP_PROG:
        prog = bpf->prog;
        if (prog) {
            /* 替换旧 program（如果存在）：必须先取出旧 prog
             * 否则 bpf_prog_inc(new) 后旧 prog 引用泄漏，永不释放 */
            bpf_prog_inc(prog);
            old = rcu_dereference(priv->xdp_prog);
            rcu_assign_pointer(priv->xdp_prog, prog);
            if (old) {
                synchronize_net();
                bpf_prog_put(old);
            }
            atomic64_inc(&priv->xdp_prog_set_count);
            netdev_info(ndev, "XDP program loaded: %s\n",
                        prog->aux->name);
        } else {
            /* 卸载 XDP program（prog == NULL）*/
            prog = rcu_dereference(priv->xdp_prog);
            rcu_assign_pointer(priv->xdp_prog, NULL);
            if (prog) {
                synchronize_net();
                bpf_prog_put(prog);
                atomic64_inc(&priv->xdp_prog_clear_count);
            }
            netdev_info(ndev, "XDP program unloaded\n");
        }
        return 0;
    default:
        return -EINVAL;
    }
}

/*========================================================
 *     stats — ndo_get_stats64 回调
 *
 *     由内核主动调用（RTNL 锁保护）：
 *       - ifconfig / ip -s link 显示统计
 *       - /proc/net/dev 读取
 *
 *     职责：将所有队列的 atomic64_t 统计聚合到 rtnl_link_stats64
 *           注意：这是一个快照读取，可能在统计更新中间读取
 *========================================================*/
static void stage14_get_stats64(struct net_device *ndev, struct rtnl_link_stats64 *stats)
{
    struct stage14_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage14_queue *q = &priv->queues[i];
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
    STAGE14_ETHTOOL_STATS_TX_PACKETS,
    STAGE14_ETHTOOL_STATS_TX_BYTES,
    STAGE14_ETHTOOL_STATS_TX_SUBMIT,
    STAGE14_ETHTOOL_STATS_TX_COMPLETE,
    STAGE14_ETHTOOL_STATS_TX_DROPPED,
    STAGE14_ETHTOOL_STATS_RX_PACKETS,
    STAGE14_ETHTOOL_STATS_RX_BYTES,
    STAGE14_ETHTOOL_STATS_RX_CONSUME,
    STAGE14_ETHTOOL_STATS_RX_DROPPED,
    STAGE14_ETHTOOL_STATS_RX_PAGE_ALLOC,
    STAGE14_ETHTOOL_STATS_RX_BUILD_SKB_FAIL,
    STAGE14_ETHTOOL_STATS_TEST_TX_SUBMIT,
    STAGE14_ETHTOOL_STATS_TEST_RX_CONSUME,
    STAGE14_ETHTOOL_STATS_TX_CSUM_PARTIAL,
    STAGE14_ETHTOOL_STATS_TX_GSO_PACKETS,
    STAGE14_ETHTOOL_STATS_RX_GRO_PACKETS,
    STAGE14_ETHTOOL_STATS_FEATURE_SET_COUNT,
    STAGE14_ETHTOOL_STATS_XDP_PASS,
    STAGE14_ETHTOOL_STATS_XDP_DROP,
    STAGE14_ETHTOOL_STATS_XDP_TX,
    STAGE14_ETHTOOL_STATS_XDP_REDIRECT,
    STAGE14_ETHTOOL_STATS_XDP_ERR,
    STAGE14_ETHTOOL_STATS_XDP_PROG_SET,
    STAGE14_ETHTOOL_STATS_XDP_PROG_CLEAR,
    STAGE14_ETHTOOL_STATS_COUNT,
};

static const char stage14_ethtool_stat_names[][ETH_GSTRING_LEN] = {
    [STAGE14_ETHTOOL_STATS_TX_PACKETS]       = "tx_packets",
    [STAGE14_ETHTOOL_STATS_TX_BYTES]         = "tx_bytes",
    [STAGE14_ETHTOOL_STATS_TX_SUBMIT]        = "tx_submit_count",
    [STAGE14_ETHTOOL_STATS_TX_COMPLETE]      = "tx_complete_count",
    [STAGE14_ETHTOOL_STATS_TX_DROPPED]       = "tx_dropped",
    [STAGE14_ETHTOOL_STATS_RX_PACKETS]       = "rx_packets",
    [STAGE14_ETHTOOL_STATS_RX_BYTES]         = "rx_bytes",
    [STAGE14_ETHTOOL_STATS_RX_CONSUME]       = "rx_consume_count",
    [STAGE14_ETHTOOL_STATS_RX_DROPPED]       = "rx_dropped",
    [STAGE14_ETHTOOL_STATS_RX_PAGE_ALLOC]    = "rx_page_alloc",
    [STAGE14_ETHTOOL_STATS_RX_BUILD_SKB_FAIL] = "rx_build_skb_fail",
    [STAGE14_ETHTOOL_STATS_TEST_TX_SUBMIT]   = "test_tx_submit",
    [STAGE14_ETHTOOL_STATS_TEST_RX_CONSUME]  = "test_rx_consume",
    [STAGE14_ETHTOOL_STATS_TX_CSUM_PARTIAL]  = "tx_csum_partial",
    [STAGE14_ETHTOOL_STATS_TX_GSO_PACKETS]   = "tx_gso_packets",
    [STAGE14_ETHTOOL_STATS_RX_GRO_PACKETS]   = "rx_gro_packets",
    [STAGE14_ETHTOOL_STATS_FEATURE_SET_COUNT] = "feature_set_count",
    [STAGE14_ETHTOOL_STATS_XDP_PASS]         = "xdp_pass",
    [STAGE14_ETHTOOL_STATS_XDP_DROP]         = "xdp_drop",
    [STAGE14_ETHTOOL_STATS_XDP_TX]           = "xdp_tx",
    [STAGE14_ETHTOOL_STATS_XDP_REDIRECT]     = "xdp_redirect",
    [STAGE14_ETHTOOL_STATS_XDP_ERR]         = "xdp_err",
    [STAGE14_ETHTOOL_STATS_XDP_PROG_SET]    = "xdp_prog_set",
    [STAGE14_ETHTOOL_STATS_XDP_PROG_CLEAR]  = "xdp_prog_clear",
};

static void stage14_get_drvinfo(struct net_device *ndev,
                                struct ethtool_drvinfo *drvinfo)
{
    strscpy(drvinfo->driver, "netdev_stage14", sizeof(drvinfo->driver));
    strscpy(drvinfo->version, "1.0", sizeof(drvinfo->version));
    strscpy(drvinfo->bus_info, "platform", sizeof(drvinfo->bus_info));
}

static void stage14_get_strings(struct net_device *ndev, u32 stringset, u8 *buf)
{
    if (stringset == ETH_SS_STATS)
        memcpy(buf, stage14_ethtool_stat_names,
               sizeof(stage14_ethtool_stat_names));
}

static int stage14_get_sset_count(struct net_device *ndev, int sset)
{
    if (sset == ETH_SS_STATS)
        return STAGE14_ETHTOOL_STATS_COUNT;
    return -EOPNOTSUPP;
}

static void stage14_get_ethtool_stats(struct net_device *ndev,
                                      struct ethtool_stats *stats,
                                      u64 *data)
{
    struct stage14_priv *priv = netdev_priv(ndev);
    int i;

    memset(data, 0, sizeof(u64) * STAGE14_ETHTOOL_STATS_COUNT);

    for (i = 0; i < priv->num_queues; i++) {
        struct stage14_queue *q = &priv->queues[i];

        /* 聚合统计：全部累加 */
        data[STAGE14_ETHTOOL_STATS_TX_PACKETS] += atomic64_read(&q->stats.tx_packets);
        data[STAGE14_ETHTOOL_STATS_TX_BYTES]  += atomic64_read(&q->stats.tx_bytes);
        data[STAGE14_ETHTOOL_STATS_TX_SUBMIT] += atomic64_read(&q->stats.tx_submit_count);
        data[STAGE14_ETHTOOL_STATS_TX_COMPLETE] += atomic64_read(&q->stats.tx_complete_count);
        data[STAGE14_ETHTOOL_STATS_TX_DROPPED] += atomic64_read(&q->stats.tx_dropped);

        data[STAGE14_ETHTOOL_STATS_RX_PACKETS] += atomic64_read(&q->stats.rx_packets);
        data[STAGE14_ETHTOOL_STATS_RX_BYTES]  += atomic64_read(&q->stats.rx_bytes);
        data[STAGE14_ETHTOOL_STATS_RX_CONSUME] += atomic64_read(&q->stats.rx_consume_count);
        data[STAGE14_ETHTOOL_STATS_RX_DROPPED] += atomic64_read(&q->stats.rx_dropped);

        data[STAGE14_ETHTOOL_STATS_RX_PAGE_ALLOC] += atomic64_read(&q->stats.pp_alloc);
        data[STAGE14_ETHTOOL_STATS_RX_BUILD_SKB_FAIL] += atomic64_read(&q->stats.pp_build_skb_fail);

        data[STAGE14_ETHTOOL_STATS_TEST_TX_SUBMIT] += atomic64_read(&q->stats.test_tx_submit_count);
        data[STAGE14_ETHTOOL_STATS_TEST_RX_CONSUME] += atomic64_read(&q->stats.test_rx_consume_count);
        data[STAGE14_ETHTOOL_STATS_TX_CSUM_PARTIAL] += atomic64_read(&q->stats.tx_csum_partial_count);
        data[STAGE14_ETHTOOL_STATS_TX_GSO_PACKETS] += atomic64_read(&q->stats.tx_gso_packets);
        data[STAGE14_ETHTOOL_STATS_RX_GRO_PACKETS] += atomic64_read(&q->stats.rx_gro_packets);
        data[STAGE14_ETHTOOL_STATS_FEATURE_SET_COUNT] += atomic64_read(&q->stats.feature_set_count);

        data[STAGE14_ETHTOOL_STATS_XDP_PASS] += atomic64_read(&q->stats.xdp.xdp_pass);
        data[STAGE14_ETHTOOL_STATS_XDP_DROP] += atomic64_read(&q->stats.xdp.xdp_drop);
        data[STAGE14_ETHTOOL_STATS_XDP_TX] += atomic64_read(&q->stats.xdp.xdp_tx);
        data[STAGE14_ETHTOOL_STATS_XDP_REDIRECT] += atomic64_read(&q->stats.xdp.xdp_redirect);
        data[STAGE14_ETHTOOL_STATS_XDP_ERR] += atomic64_read(&q->stats.xdp.xdp_err);
    }
    data[STAGE14_ETHTOOL_STATS_XDP_PROG_SET] = atomic64_read(&priv->xdp_prog_set_count);
    data[STAGE14_ETHTOOL_STATS_XDP_PROG_CLEAR] = atomic64_read(&priv->xdp_prog_clear_count);
}

static void stage14_get_ringparam(struct net_device *ndev,
                                  STAGE14_ETHTOOL_RINGPARAM_ARGS)
{
    struct stage14_priv *priv = netdev_priv(ndev);

    ringparam->rx_max_pending = priv->ring_size;
    ringparam->rx_pending = priv->ring_size;
    ringparam->tx_max_pending = priv->ring_size;
    ringparam->tx_pending = priv->ring_size;
}

static int stage14_set_ringparam(struct net_device *ndev,
                                 STAGE14_ETHTOOL_RINGPARAM_ARGS)
{
    struct stage14_priv *priv = netdev_priv(ndev);
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

static void stage14_get_channels(struct net_device *ndev,
                                 struct ethtool_channels *channels)
{
    struct stage14_priv *priv = netdev_priv(ndev);

    channels->max_rx = priv->num_queues;
    channels->max_tx = priv->num_queues;
    channels->rx_count = priv->num_queues;
    channels->tx_count = priv->num_queues;
}

static int stage14_set_channels(struct net_device *ndev,
                                 struct ethtool_channels *channels)
{
    struct stage14_priv *priv = netdev_priv(ndev);
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

static u32 stage14_get_priv_flags(struct net_device *ndev)
{
    struct stage14_priv *priv = netdev_priv(ndev);
    return priv->ethtool_priv_flags;
}

static int stage14_set_priv_flags(struct net_device *ndev, u32 flags)
{
    struct stage14_priv *priv = netdev_priv(ndev);
    priv->ethtool_priv_flags = flags;
    return 0;
}

/*========================================================
 *     ethtool_ops — 标准的 ethtool 接口实现
 *
 *     ethtool 是 Linux 网络设备的控制面工具：
 *       ethtool -S eth0    → get_ethtool_stats（显示统计）
 *       ethtool -k eth0    → 查询 offload 能力（hw_features）
 *       ethtool -K eth0 gro on → set_features（开关 offload）
 *       ethtool -g eth0    → get_ringparam（ring 大小）
 *       ethtool -L eth0    → set_channels（队列数）
 *
 *     stats 导出机制：
 *       - get_sset_count() 返回统计项数量
 *       - get_strings() 返回统计项名称（ETH_SS_STATS）
 *       - get_ethtool_stats() 填充 u64 data[] 数组
 *       - 内核自动处理 ifconfig / ip -s link 的显示
 *========================================================*/
static const struct ethtool_ops stage14_ethtool_ops = {
    .get_drvinfo        = stage14_get_drvinfo,
    .get_strings        = stage14_get_strings,
    .get_sset_count     = stage14_get_sset_count,
    .get_ethtool_stats  = stage14_get_ethtool_stats,
    .get_ringparam      = stage14_get_ringparam,
    .set_ringparam      = stage14_set_ringparam,
    .get_channels       = stage14_get_channels,
    .set_channels       = stage14_set_channels,
    .get_priv_flags     = stage14_get_priv_flags,
    .set_priv_flags     = stage14_set_priv_flags,
    .get_link           = ethtool_op_get_link,
};

/*========================================================
 *     ndo_set_features — offload 特性协商
 *
 *     内核调用路径：
 *       ethtool -K eth0 gro on
 *           → ethtool_set_features()
 *           → ndo_set_features(dev, features)
 *
 *     功能：
 *       - 更新 ndev->features（应用新的 offload 开关状态）
 *       - 记录 feature_set_count（统计协商次数）
 *       - 记录 last_features（保存变更前的状态）
 *
 *     注意：软模型不实际改变 offload 处理路径，仅记录
 *========================================================*/
static int stage14_set_features(struct net_device *ndev, netdev_features_t features)
{
    struct stage14_priv *priv = netdev_priv(ndev);
    int i;
    priv->last_features = features;
    for (i = 0; i < priv->num_queues; ++i)
        atomic64_inc(&priv->queues[i].stats.feature_set_count);
    ndev->features = features;
    return 0;
}

/*========================================================
 *     netdev_ops — 驱动核心回调函数表
 *
 *     netdev_ops 是 netdev 的方法表，内核在适当时候调用：
 *       ndo_open          — 接口 UP 时调用（ifconfig up / ip link set up）
 *       ndo_stop          — 接口 DOWN 时调用（ifconfig down）
 *       ndo_start_xmit    — 数据包发送（netif_tx → dev_queue_xmit 路径）
 *       ndo_select_queue  — 多队列选择队列（skb_get_queue_mapping）
 *       ndo_get_stats64   — 读取统计（ifconfig / ip -s link）
 *       ndo_set_features  — offload 特性协商（ethtool -K）
 *       ndo_bpf          — XDP program 注册（ip link set xdp obj）
 *========================================================*/
static const struct net_device_ops stage14_netdev_ops = {
    .ndo_open = stage14_open,
    .ndo_stop = stage14_stop,
    .ndo_start_xmit = stage14_start_xmit,
    .ndo_select_queue = stage14_select_queue,
    .ndo_get_stats64 = stage14_get_stats64,
    .ndo_set_features = stage14_set_features,
    .ndo_bpf = stage14_xdp,
};

/*========================================================
 *     debugfs 调试接口
 *
 *     debugfs 是内核提供的轻量调试文件系统（通常挂载在 /sys/kernel/debug）
 *     通过 seq_file 接口导出结构化数据（类似 /proc）
 *
 *     本驱动导出的 debugfs 文件：
 *       - stats   — 各队列收发统计、timeline、backend 状态
 *       - queues — TX/RX ring 槽位状态快照（仅显示前 N 个）
 *       - timeline — 时间线延迟统计（doorbell→backend→IRQ→poll）
 *       - vectors — MSI-X 向量（每队列）的中断触发/处理统计
 *       - page_pool — 每队列 page_pool 分配/回收统计
 *       - offload  — offload 特性开关状态
 *       - xdp     — XDP program 注册状态 + 各 action 统计
 *
 *     查看方式：sudo cat /sys/kernel/debug/netdev_stage14_soft/xdp
 *========================================================*/
static int stage14_stats_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage14_priv *priv = netdev_priv(ndev);
    struct bpf_prog *prog;
    int i;

    rcu_read_lock();
    prog = rcu_dereference(priv->xdp_prog);
    seq_printf(m, "ifname=%s num_queues=%u ring_size=%u napi_weight=%u backend_batch=%u open=%lld stop=%lld xdp_prog=%p\n",
               ndev->name, priv->num_queues, priv->ring_size, priv->napi_weight,
               priv->backend_batch, atomic64_read(&priv->open_count),
               atomic64_read(&priv->stop_count), prog);
    rcu_read_unlock();
    for (i = 0; i < priv->num_queues; ++i) {
        struct stage14_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx_submit=%lld tx_complete=%lld tx_packets=%lld tx_bytes=%lld tx_busy=%lld tx_drop=%lld "
                   "rx_post=%lld rx_ready=%lld rx_consume=%lld rx_packets=%lld rx_bytes=%lld rx_drop=%lld "
                   "doorbell=%lld backend_schedule=%lld backend_run=%lld backend_tx=%lld backend_rx=%lld "
                   "irq=%lld napi_poll=%lld napi_complete=%lld napi_work=%lld test_tx=%lld test_rx=%lld "
                   "tx_csum_partial=%lld tx_gso=%lld rx_gro=%lld feature_set=%lld "
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
                   atomic64_read(&q->stats.backend_schedule_count),
                   atomic64_read(&q->stats.backend_run_count),
                   atomic64_read(&q->stats.backend_tx_processed),
                   atomic64_read(&q->stats.backend_rx_produced),
                   atomic64_read(&q->stats.irq_count),
                   atomic64_read(&q->stats.napi_poll_count),
                   atomic64_read(&q->stats.napi_complete_count),
                   atomic64_read(&q->stats.napi_work_total),
                   atomic64_read(&q->stats.test_tx_submit_count),
                   atomic64_read(&q->stats.test_rx_consume_count),
                   atomic64_read(&q->stats.tx_csum_partial_count),
                   atomic64_read(&q->stats.tx_gso_packets),
                   atomic64_read(&q->stats.rx_gro_packets),
                   atomic64_read(&q->stats.feature_set_count),
                   atomic64_read(&q->stats.pp_alloc),
                   atomic64_read(&q->stats.pp_recycle),
                   atomic64_read(&q->stats.pp_build_skb_fail));
    }
    return 0;
}

static int stage14_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage14_stats_show, inode->i_private);
}

static const struct file_operations stage14_stats_fops = {
    .owner = THIS_MODULE,
    .open = stage14_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage14_queues_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage14_priv *priv = netdev_priv(ndev);
    int i, j;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage14_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx submit=%u notify=%u complete=%u inflight=%u done=%u | "
                   "rx post=%u device=%u consume=%u posted=%u ready=%u "
                   "irq_masked=%u doorbell_pending=%u backend_running=%u\n",
                   q->qid, q->txq.submit_idx, q->txq.notify_idx, q->txq.complete_idx,
                   q->tx_inflight, q->tx_done, q->rxq.post_idx, q->rxq.device_idx,
                   q->rxq.consume_idx, q->rx_posted, q->rx_ready,
                   q->irq_masked, q->doorbell_pending, q->backend_running);
        for (j = 0; j < min_t(u16, q->txq.size, STAGE14_QUEUE_DUMP_LIMIT); ++j)
            seq_printf(m, "  q%u txslot[%d]: state=%u len=%u\n", q->qid, j,
                       q->txq.slots[j].state, q->txq.slots[j].data_len);
        for (j = 0; j < min_t(u16, q->rxq.size, STAGE14_QUEUE_DUMP_LIMIT); ++j)
            seq_printf(m, "  q%u rxslot[%d]: state=%u len=%u has_page=%u\n", q->qid, j,
                       q->rxq.slots[j].state, q->rxq.slots[j].data_len,
                       q->rxq.slots[j].page ? 1 : 0);
    }
    return 0;
}

static int stage14_queues_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage14_queues_show, inode->i_private);
}

static const struct file_operations stage14_queues_fops = {
    .owner = THIS_MODULE,
    .open = stage14_queues_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage14_timeline_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage14_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage14_queue *q = &priv->queues[i];
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

static int stage14_timeline_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage14_timeline_show, inode->i_private);
}

static const struct file_operations stage14_timeline_fops = {
    .owner = THIS_MODULE,
    .open = stage14_timeline_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage14_vectors_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage14_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage14_vector *vec = &priv->vectors[i];
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

static int stage14_vectors_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage14_vectors_show, inode->i_private);
}

static const struct file_operations stage14_vectors_fops = {
    .owner = THIS_MODULE,
    .open = stage14_vectors_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage14_pp_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage14_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage14_queue *q = &priv->queues[i];
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

static int stage14_pp_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage14_pp_show, inode->i_private);
}

static const struct file_operations stage14_pp_fops = {
    .owner = THIS_MODULE,
    .open = stage14_pp_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};


static int stage14_offload_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage14_priv *priv = netdev_priv(ndev);
    seq_printf(m, "ifname=%s features=0x%llx hw_csum=%d rx_csum=%d sg=%d gso_sw=%d gro_enabled=%d last_features=0x%llx xdp_prog=%p\n",
               ndev->name, (unsigned long long)ndev->features,
               !!(ndev->features & NETIF_F_HW_CSUM), !!(ndev->features & NETIF_F_RXCSUM),
               !!(ndev->features & NETIF_F_SG), !!(ndev->features & NETIF_F_GSO_SOFTWARE),
               !!(ndev->features & NETIF_F_GRO), (unsigned long long)priv->last_features,
               priv->xdp_prog);
    return 0;
}

static int stage14_offload_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage14_offload_show, inode->i_private);
}

static const struct file_operations stage14_offload_fops = {
    .owner = THIS_MODULE,
    .open = stage14_offload_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/*========================================================
 *     debugfs xdp 状态文件
 *
 *     显示内容：
 *       - xdp_prog：当前注册的 BPF program 地址（null = 未加载）
 *       - prog_set / prog_clear：program 加载/卸载次数
 *       - qN xdp_pass/drop/tx/redirect/err：每队列 XDP action 统计
 *
 *     注意：读取 xdp_prog 时使用 rcu_read_lock() 保护
 *           因为 ndo_bpf 可能与 debugfs 读取并发执行
 *========================================================*/
static int stage14_xdp_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage14_priv *priv = netdev_priv(ndev);
    struct bpf_prog *prog;
    int i;

    rcu_read_lock();
    prog = rcu_dereference(priv->xdp_prog);
    seq_printf(m, "xdp_prog=%p prog_set=%lld prog_clear=%lld\n",
               prog, atomic64_read(&priv->xdp_prog_set_count),
               atomic64_read(&priv->xdp_prog_clear_count));
    rcu_read_unlock();
    for (i = 0; i < priv->num_queues; ++i) {
        struct stage14_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: xdp_pass=%lld xdp_drop=%lld xdp_tx=%lld xdp_redirect=%lld xdp_err=%lld\n",
                   q->qid,
                   atomic64_read(&q->stats.xdp.xdp_pass),
                   atomic64_read(&q->stats.xdp.xdp_drop),
                   atomic64_read(&q->stats.xdp.xdp_tx),
                   atomic64_read(&q->stats.xdp.xdp_redirect),
                   atomic64_read(&q->stats.xdp.xdp_err));
    }
    return 0;
}

static int stage14_xdp_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage14_xdp_show, inode->i_private);
}

static const struct file_operations stage14_xdp_fops = {
    .owner = THIS_MODULE,
    .open = stage14_xdp_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static void stage14_debugfs_init(struct stage14_priv *priv)
{
    priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
    if (!priv->dbg_dir)
        return;
    debugfs_create_file("stats", 0444, priv->dbg_dir, priv->ndev, &stage14_stats_fops);
    debugfs_create_file("queues", 0444, priv->dbg_dir, priv->ndev, &stage14_queues_fops);
    debugfs_create_file("timeline", 0444, priv->dbg_dir, priv->ndev, &stage14_timeline_fops);
    debugfs_create_file("vectors", 0444, priv->dbg_dir, priv->ndev, &stage14_vectors_fops);
    debugfs_create_file("page_pool", 0444, priv->dbg_dir, priv->ndev, &stage14_pp_fops);
    debugfs_create_file("offload", 0444, priv->dbg_dir, priv->ndev, &stage14_offload_fops);
    debugfs_create_file("xdp", 0444, priv->dbg_dir, priv->ndev, &stage14_xdp_fops);
}

static void stage14_debugfs_deinit(struct stage14_priv *priv)
{
    debugfs_remove_recursive(priv->dbg_dir);
    priv->dbg_dir = NULL;
}

/*========================================================
 *     模块初始化 / 退出
 *
 *     初始化顺序（重要！资源分配必须与释放对称）：
 *       1. alloc_etherdev_mqs() — 分配 netdev + priv（needs_free_netdev=true）
 *       2. 设置 netdev_ops / ethtool_ops / features
 *       3. spin_lock_init / priv 字段初始化
 *       4. alloc_workqueue — backend_wq / irq_wq
 *       5. per-queue: alloc_ring → netif_napi_add → page_pool → xdp_rxq_init
 *       6. register_netdev() — 创建设备节点
 *       7. debugfs_init() — 创建调试接口
 *
 *     退出顺序（与初始化严格对称，倒序释放）：
 *       1. debugfs_deinit()
 *       2. netif_tx_disable() + cancel_work_sync() + napi_disable()
 *       3. unregister_netdev() — 销毁设备节点
 *       4. xdp_rxq_deinit() + free_ring() + destroy_page_pool()
 *       5. destroy_workqueue()
 *       6. free_netdev() — 由 needs_free_netdev 机制自动完成
 *
 *     为什么 napi_disable 必须在 unregister_netdev 之前？
 *       - unregister_netdev 内部也会调 napi_disable
 *       - 如果先调 unregister_netdev，内部会等待 napi（已 disable 状态）
 *         导致等待永远不会完成（嵌套死锁）
 *========================================================*/
static int __init stage14_soft_init(void)
{
    struct net_device *ndev;
    struct stage14_priv *priv;
    int i, ret;

    num_queues = clamp_t(unsigned int, num_queues, 1, STAGE14_MAX_QUEUES);
    ring_size = max_t(unsigned int, ring_size, 32);
    napi_weight = max_t(unsigned int, napi_weight, 16);
    backend_batch = max_t(unsigned int, backend_batch, 1);

    ndev = alloc_etherdev_mqs(sizeof(struct stage14_priv), num_queues, num_queues);
    if (!ndev)
        return -ENOMEM;

    strscpy(ndev->name, ifname, IFNAMSIZ);
    ndev->netdev_ops = &stage14_netdev_ops;
    ndev->ethtool_ops = &stage14_ethtool_ops;
    ndev->hw_features = NETIF_F_RXCSUM | NETIF_F_HW_CSUM | NETIF_F_SG | NETIF_F_GSO_SOFTWARE | NETIF_F_GRO;
    ndev->features |= ndev->hw_features;
    ndev->wanted_features = ndev->features;
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
    priv->last_features = ndev->features;
    spin_lock_init(&priv->state_lock);

    priv->backend_wq = alloc_workqueue("stage14s_backend", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!priv->backend_wq) {
        free_netdev(ndev);
        return -ENOMEM;
    }
    priv->irq_wq = alloc_workqueue("stage14s_irq", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!priv->irq_wq) {
        destroy_workqueue(priv->backend_wq);
        free_netdev(ndev);
        return -ENOMEM;
    }

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage14_queue *q = &priv->queues[i];
        q->priv = priv;
        q->qid = i;
        q->vector_id = i;
        INIT_WORK(&q->backend_work, stage14_backend_workfn);
        INIT_WORK(&q->irq_work, stage14_irq_workfn);
        ret = stage14_alloc_ring(&q->txq, ring_size);
        if (ret)
            goto err;
        ret = stage14_alloc_ring(&q->rxq, ring_size);
        if (ret)
            goto err;
        STAGE14_NETIF_NAPI_ADD(ndev, &q->napi, stage14_napi_poll, napi_weight);

        /* 每队列独立 page_pool */
        q->pp = stage14_create_page_pool(q);
        if (!q->pp) {
            ret = -ENOMEM;
            goto err;
        }

        /* XDP RXQ info（必须在 page_pool 创建之后） */
        ret = stage14_xdp_rxq_init(q);
        if (ret)
            goto err;

        /* vector 初始化 */
        priv->vectors[i].vector_id = i;
        priv->vectors[i].qid = i;
        priv->vectors[i].target_cpu = stage14_pick_irq_cpu(i);
        snprintf(priv->vectors[i].name, sizeof(priv->vectors[i].name), "stage14s-q%u", i);
        priv->vectors[i].last_raise_cpu = -1;
        priv->vectors[i].last_handle_cpu = -1;

        stage14_reset_queue(q);
    }

    ret = register_netdev(ndev);
    if (ret)
        goto err;

    stage14_debugfs_init(priv);
    stage14_soft_ndev = ndev;
    pr_info("%s: loaded ifname=%s num_queues=%u ring_size=%u napi_weight=%u backend_batch=%u\n",
            DRV_NAME, ndev->name, priv->num_queues, priv->ring_size,
            priv->napi_weight, priv->backend_batch);
    return 0;
err:
    /* 与 stage14_soft_exit 对称：先 napi_disable 再 free_ring */
    for (i = 0; i < priv->num_queues; ++i) {
        if (priv->queues[i].napi.dev)
            napi_disable(&priv->queues[i].napi);
    }
    for (i = 0; i < priv->num_queues; ++i) {
        if (priv->queues[i].napi.dev)
            netif_napi_del(&priv->queues[i].napi);
        stage14_xdp_rxq_deinit(&priv->queues[i]);
        stage14_free_ring(ndev, &priv->queues[i].txq, NULL, false);
        stage14_free_ring(ndev, &priv->queues[i].rxq, priv->queues[i].pp, true);
        stage14_destroy_page_pool(&priv->queues[i]);
    }
    if (priv->irq_wq)
        destroy_workqueue(priv->irq_wq);
    destroy_workqueue(priv->backend_wq);
    free_netdev(ndev);
    return ret;
}

static void __exit stage14_soft_exit(void)
{
    struct stage14_priv *priv;
    int i;

    if (!stage14_soft_ndev)
        return;
    priv = netdev_priv(stage14_soft_ndev);
    stage14_debugfs_deinit(priv);
    /* 正确顺序：
     * 1. netif_tx_disable 防止新 xmit
     * 2. cancel_work_sync 确保 backend/irq work 不再运行
     * 3. napi_disable 等待 NAPI kthread 完全退出（必须在 unregister_netdev 之前，
     *    否则 unregister_netdev 内部也会调 napi_disable，形成嵌套等待死锁风险）
     * 4. unregister_netdev（内部会清理剩余的 NAPI 实例）
     * 5. destroy_workqueue / free_ring / page_pool */
    netif_tx_disable(stage14_soft_ndev);
    for (i = 0; i < priv->num_queues; ++i) {
        cancel_work_sync(&priv->queues[i].backend_work);
        cancel_work_sync(&priv->queues[i].irq_work);
    }
    for (i = 0; i < priv->num_queues; ++i)
        napi_disable(&priv->queues[i].napi);
    unregister_netdev(stage14_soft_ndev);

    /* 卸载 XDP program（RCU 保护） */
    {
        struct bpf_prog *old_prog = rcu_dereference(priv->xdp_prog);
        rcu_assign_pointer(priv->xdp_prog, NULL);
        if (old_prog) {
            synchronize_net();
            bpf_prog_put(old_prog);
        }
    }

    destroy_workqueue(priv->backend_wq);
    destroy_workqueue(priv->irq_wq);
    for (i = 0; i < priv->num_queues; ++i) {
        stage14_free_ring(stage14_soft_ndev, &priv->queues[i].txq, NULL, false);
        stage14_free_ring(stage14_soft_ndev, &priv->queues[i].rxq, priv->queues[i].pp, true);
        stage14_destroy_page_pool(&priv->queues[i]);
    }
    free_netdev(stage14_soft_ndev);
    stage14_soft_ndev = NULL;
    pr_info("%s: unloaded\n", DRV_NAME);
}

module_init(stage14_soft_init);
module_exit(stage14_soft_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Lab");
MODULE_DESCRIPTION("stage14 soft page_pool netdev with XDP");