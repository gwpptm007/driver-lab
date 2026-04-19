// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage09.c — stage09_multi_queue_scaling 教学驱动
 *
 * 核心目标：
 * - 在 stage08 单队列异步 backend 的基础上，引入多 queue
 * - 每 queue 独立 NAPI / backend work / 统计 / timeline
 * - 提供简单 queue 分发策略，观察多 queue 分布
 *
 * 【学习要点】
 *
 * 1. 多队列架构的核心数据结构
 *    stage09_priv 管理 queues[] 数组，每个 stage09_queue 包含：
 *    - txq/rxq：独立 TX/RX ring
 *    - napi：独立 NAPI poll 函数
 *    - backend_work：独立 backend work item
 *    - timeline/stats：独立统计
 *    这使得每个队列的行为完全隔离，类似真实 NIC 的多队列
 *
 * 2. 队列分发策略（stage09_select_queue）
 *    - 优先使用 skb_get_hash() 做 hash % num_queues
 *    - 无 hash 时用 round-robin（rr_counter 原子递增）
 *    - 真实 NIC 通常用 RSS（Receive Side Scaling）做更复杂分发
 *
 * 3. alloc_etherdev_mqs() vs alloc_netdev()
 *    - alloc_netdev() + 手动 ether_setup()：单队列（stage08 方式）
 *    - alloc_etherdev_mqs(txqs, rxqs)：内核自动创建指定数量的 TX/RX 队列
 *    - 内核内部会为每个队列分配 netdev->_tx + 结构，实际队列数受限于 num_possible_rings()
 *
 * 4. per-queue timeline 的意义
 *    stage08 只有全局 timeline；stage09 每个队列有独立 timeline
 *    可以观测队列 0 和队列 1 的 doorbell_to_backend delta 是否一致
 *
 * 5. 统一 backend_wq vs per-queue wq
 *    - stage09 用一个统一 workqueue（WQ_UNBOUND）调度所有队列的 backend work
 *    - WQ_UNBOUND：work 不绑定特定 CPU，让调度器决定在哪执行
 *    - 每个队列的 backend_work 独立入队，互不影响
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
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/jhash.h>

#include "../include/netdev_stage09_compat.h"

#define DRV_NAME "netdev_stage09"
#define STAGE09_MAX_QUEUES 4
#define STAGE09_DEFAULT_NUM_QUEUES 2
#define STAGE09_DEFAULT_RING_SIZE 128
#define STAGE09_DEFAULT_NAPI_WEIGHT 64
#define STAGE09_DEFAULT_RX_BUF_SIZE 2048
#define STAGE09_DEFAULT_BACKEND_BATCH 64
#define STAGE09_QUEUE_DUMP_LIMIT 8
#define STAGE09_TEST_PROTO 0x88B9
#define STAGE09_TEST_MAGIC "STAGE09"
/* 【学习】
 * STAGE09_MAX_QUEUES=4：最大支持 4 个队列（硬件上限）
 * STAGE09_DEFAULT_NUM_QUEUES=2：默认开启 2 个队列（起步规模）
 * STAGE09_TEST_PROTO=0x88B9：测试帧以太网类型（教学专用，避免与真实协议冲突）
 * STAGE09_QUEUE_DUMP_LIMIT=8：debugfs queues 文件每队列最多打印 8 个 slot（防止过长）
 */

enum stage09_slot_state {
/* 【学习】TX slot 状态机：FREE → SUBMITTED → DONE → FREE
 * RX slot 状态机：FREE → POSTED → DONE → FREE
 * 两种 ring 共享同一状态枚举，因为语义相同
 */
    S09_SLOT_FREE = 0,      /* TX: 可用；RX: 可填充 */
    S09_SLOT_POSTED,        /* RX: buffer 已 posted，等待 backend 生产 */
    S09_SLOT_SUBMITTED,     /* TX: frame 已提交，等待 backend 处理 */
    S09_SLOT_DONE,          /* TX: backend 处理完，等待 NAPI 回收；RX: 数据就绪，等待 NAPI 消费 */
};

struct stage09_desc {
/* 【学习】descriptor 是 DMA 环的描述符，直接写入硬件
 * 与 buf_slot 不同，desc 只包含 DMA 需要的最小信息
 * 真实 virtio-net 的 vring_desc 就是这个结构
 */
    dma_addr_t dma_addr;  /* 数据的 DMA 总线地址 */
    u32 data_len;        /* 数据长度（字节） */
    u16 state;           /* 当前状态（enum stage09_slot_state） */
    u16 flags;           /* 保留，备用 */
};

struct stage09_buf_slot {
/* 【学习】buf_slot 是 skb 的容器，挂在 ring 上管理生命周期
 * state 字段与 desc->state 同步（两处都要改，保持一致）
 * last_seq 用于检测帧顺序（教学用途）
 */
    struct sk_buff *skb;   /* 关联的 socket buffer */
    dma_addr_t dma_addr;   /* skb->data 的 DMA 地址 */
    u32 buf_len;           /* buffer 总长度（skb->truesize） */
    u32 data_len;           /* 实际数据长度 */
    u16 state;             /* slot 状态（必须与 desc->state 一致） */
    u16 id;                /* slot 索引，用于调试 */
    u32 last_seq;          /* 最近一次 test 帧的 sequence number */
};

struct stage09_ring {
/* 【学习】TX ring 的 6 个 index：
 * submit_idx：下一个可用的 TX slot（生产者视角）
 * notify_idx：backend 已通知处理到哪个 slot（正在处理）
 * complete_idx：NAPI 已回收到哪个 slot（已清理）
 * inflight：submit_idx - complete_idx（飞行中帧数）
 *
 * RX ring 的 6 个 index：
 * post_idx：下一个可填充 RX buffer 的位置
 * device_idx：backend 生产到哪个 slot（正在写）
 * consume_idx：NAPI 已消费到哪个 slot
 * posted：已填充未处理的 RX buffer 数
 *
 * 这些 index 两两配合形成双生产者/双消费者模型
 */
    struct stage09_desc *desc;      /* DMA descriptor 数组 */
    struct stage09_buf_slot *slots; /* skb + meta 数组 */
    u16 size;             /* ring 容量（通常 128） */
    u16 submit_idx;        /* TX: 下一个可用 slot */
    u16 notify_idx;        /* TX: backend 通知处理到哪 */
    u16 complete_idx;       /* TX: NAPI 已回收到哪里 */
    u16 post_idx;           /* RX: 下一个填充位置 */
    u16 device_idx;         /* RX: backend 生产到哪 */
    u16 consume_idx;        /* RX: NAPI 消费到哪里 */
};

struct stage09_timeline {
/* 【学习】每个队列独立 timeline，记录最近一次完整事务的 8 个时间戳
 * 与 stage08 不同，stage09 每个队列都有自己的一套时间戳
 * 通过 delta 可以观测每个队列的异步延迟分布
 *
 * 完整 TX→RX 事务的时间戳链：
 * last_submit_ns → last_doorbell_ns → last_backend_wakeup_ns → last_backend_done_ns
 * → last_irq_ns → last_poll_ns → last_complete_ns → last_consume_ns
 *
 * 关键 delta 解读：
 * - submit_to_doorbell_ns：submit 到 doorbell（同一上下文，应 ~140ns）
 * - doorbell_to_backend_ns：doorbell 到 backend 执行（异步核心，>0 证明异步）
 * - backend_to_irq_ns：backend 处理完到 irq（应 ~70ns）
 * - irq_to_poll_ns：irq 到 NAPI poll（应 ~2μs）
 */
    u64 last_submit_ns;         /* ndo_start_xmit 被调用的时刻 */
    u64 last_doorbell_ns;       /* doorbell 敲下的时刻 */
    u64 last_backend_wakeup_ns; /* backend_workfn 开始执行的时刻 */
    u64 last_backend_done_ns;   /* backend_workfn 处理完当前批次的时刻 */
    u64 last_irq_ns;            /* 硬件中断发生的时刻 */
    u64 last_poll_ns;            /* NAPI poll() 被调用的时刻 */
    u64 last_complete_ns;        /* TX completion 释放 skb 的时刻 */
    u64 last_consume_ns;         /* RX 帧被 consume 的时刻 */
};

struct stage09_queue_stats {
/* 【学习】per-queue 统计 Counter
 * 使用 atomic64_t 而非原子操作，因为每次 +-1 用原语足够
 * 与 stage08 全局 stats 不同，这里每个队列独立计数
 *
 * TX 路径统计（发送端）：
 * tx_submit_count：提交到 ring 的 TX 帧数
 * tx_complete_count：NAPI 回收的 TX 帧数
 * tx_packets：成功映射的 TX 帧数（不含 dropped）
 * tx_bytes：TX 总字节数
 * tx_busy：因 ring full 返回 NETDEV_TX_BUSY 的次数
 * tx_dropped：丢弃的 TX 帧数（含 linearize fail / dma fail）
 * tx_linearize_count：skb linearize 失败次数（非线性 skb 才需要）
 * tx_dma_map_ok/fail：DMA 映射成功/失败次数
 *
 * RX 路径统计（接收端）：
 * rx_post_count：RX buffer 被 posted 到 ring 的次数
 * rx_consume_count：NAPI poll 消费的 RX 帧数
 * rx_packets：交付给协议栈的 RX 帧数
 * rx_bytes：RX 总字节数
 * rx_dropped：丢弃的 RX 帧数
 * rx_dma_map_ok/fail：RX DMA 映射成功/失败
 *
 * Backend 路径统计：
 * doorbell_count：doorbell 敲击次数
 * backend_schedule_count：backend work 入队次数
 * backend_run_count：backend_workfn 实际执行的次数
 * backend_tx_processed：backend 处理的 TX descriptor 数
 * backend_rx_produced：backend 生产的 RX 帧数
 *
 * 中断/NAPI 统计：
 * irq_count：物理中断发生次数
 * napi_poll_count：napi->poll() 被调用的次数
 * napi_complete_count：napi_complete_done() 被调用的次数
 * napi_work_total：NAPI poll 总共处理的工作量（budget 消耗）
 *
 * 测试专用统计（test_tx/rx）：
 * 用于精确计数本次测试帧，排除历史干扰
 */
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

    atomic64_t test_tx_submit_count;  /* 测试帧专用：TX 提交数 */
    atomic64_t test_rx_consume_count; /* 测试帧专用：RX 消费数 */
};

struct stage09_priv;

struct stage09_queue {
/* 【学习】每个队列的独立执行上下文
 * stage09 的核心设计：每个队列都有完整的 Front-end → Back-end → NAPI 数据通路
 * 这模拟了真实 NIC 多队列的独立硬件通道
 *
 * Front-end（ndo_start_xmit）: 持有 priv->state_lock
 * Back-end（backend_workfn）: 持有 priv->state_lock，处理 TX/RX
 * NAPI（napi_poll）: 持有 priv->state_lock，收 TX / 消费 RX
 *
 * doorbell_pending：doorbell 握手信号，通知"有事要处理但还没处理完"
 * backend_running：防止 backend 在上一个 work 还没执行完时被重复入队
 * irq_masked：NAPI 已完成但还有未处理 work 时 mask 中断
 *
 * 与 stage08 对比：
 * stage08 只有一个全局队列；stage09 每个队列结构完全独立
 */
    struct stage09_priv *priv;
    u16 qid;                      /* 队列 ID（0, 1, 2, ...） */
    struct napi_struct napi;      /* 独立 NAPI 结构，每队列一个 */
    struct work_struct backend_work; /* 独立 backend work item */
    bool irq_masked;              /* 中断是否被 mask */
    bool doorbell_pending;        /* 是否有待处理 doorbell */
    bool backend_running;         /* backend 是否正在执行 */
    u16 tx_inflight;              /* TX 飞行中帧数 */
    u16 tx_done;                  /* TX done 环上待回收帧数 */
    u16 rx_posted;                /* 已填充待 backend 消费的 RX buffer 数 */
    u16 rx_ready;                 /* 已完成待 NAPI 消费的 RX 帧数 */
    struct stage09_ring txq;     /* TX ring（submit → notify → complete） */
    struct stage09_ring rxq;      /* RX ring（post → device → consume） */
    struct stage09_timeline timeline; /* 本队列独立 timeline */
    struct stage09_queue_stats stats; /* 本队列独立统计 */
};

struct stage09_priv {
/* 【学习】驱动私有数据，挂在 netdev->priv 上
 * priv 管理所有队列和全局资源
 *
 * state_lock：保护所有队列状态的全局锁
 * - 因为 ndo_start_xmit / backend_workfn / napi_poll 三个上下文会并发访问
 * - 每队列的 tx_inflight/rx_ready 等都需要锁保护
 *
 * backend_wq：统一 workqueue，但每个队列有独立的 backend_work item
 * - WQ_UNBOUND：work 不绑定特定 CPU，调度器决定执行位置
 * - WQ_MEM_RECLAIM：当内存紧张时，workqueue 会参与内存回收
 *
 * rr_counter：round-robin 分发计数器，当 skb 无 hash 时使用
 *
 * open_count / stop_count：设备 up/down 次数统计
 */
    struct net_device *ndev;
    spinlock_t state_lock;             /* 全局状态锁，保护所有队列状态 */
    struct workqueue_struct *backend_wq; /* 统一 backend workqueue（所有队列共享） */
    struct dentry *dbg_dir;            /* debugfs 目录句柄 */
    u32 num_queues;                    /* 实际启用的队列数 */
    u32 ring_size;                     /* 每个 ring 的 slot 数 */
    u32 napi_weight;                   /* NAPI poll 的 budget */
    u32 rx_buf_size;                   /* RX buffer 大小（字节） */
    u32 backend_delay_us;              /* backend 处理延迟（微秒，可模拟真实设备延迟） */
    u32 backend_batch;                 /* backend 每次最多处理的帧数 */
    atomic64_t rr_counter;             /* round-robin 分发计数器 */
    atomic64_t open_count;            /* 设备 up 次数 */
    atomic64_t stop_count;             /* 设备 down 次数 */
    struct stage09_queue queues[STAGE09_MAX_QUEUES]; /* 队列数组（最多 MAX_QUEUES） */
};

static char ifname[IFNAMSIZ] = "nds9";
module_param_string(ifname, ifname, sizeof(ifname), 0444);
/* 【学习】ifname: 设备名称，默认为 nds9，可通过 insmod ifname=nds9 覆盖 */

static unsigned int num_queues = STAGE09_DEFAULT_NUM_QUEUES;
module_param(num_queues, uint, 0444);
/* 【学习】num_queues: 启用队列数量，默认 2，最大 4
 * insmod num_queues=4 可开启 4 个队列（测试多队列分发） */

static unsigned int ring_size = STAGE09_DEFAULT_RING_SIZE;
module_param(ring_size, uint, 0444);
/* 【学习】ring_size: 每个 TX/RX ring 的 slot 数（默认 128）
 * 注意：TX 和 RX 共享同一个 ring_size 值 */

static unsigned int napi_weight = STAGE09_DEFAULT_NAPI_WEIGHT;
module_param(napi_weight, uint, 0444);
/* 【学习】napi_weight: NAPI poll 的 budget，默认 64
 * budget 影响每次 NAPI poll 最多处理多少帧，太大可能导致实时性下降 */

static unsigned int rx_buf_size = STAGE09_DEFAULT_RX_BUF_SIZE;
module_param(rx_buf_size, uint, 0444);
/* 【学习】rx_buf_size: RX buffer 大小（字节），默认 2048
 * 影响每次传输的最大帧长，超过则丢帧 */

static unsigned int backend_delay_us;
module_param(backend_delay_us, uint, 0644);
/* 【学习】backend_delay_us: backend 处理每批帧的延迟（微秒）
 * 默认 0（无延迟），设为 >0 可模拟真实硬件的异步处理延迟
 * 设为 100 可观测到明显的 doorbell→backend 异步阶梯 */

static unsigned int backend_batch = STAGE09_DEFAULT_BACKEND_BATCH;
module_param(backend_batch, uint, 0444);
/* 【学习】backend_batch: backend 每次最多处理的帧数
 * 默认 64，设为 1 可观察多次 schedule；设为 128 与 ring_size 等同 */

static struct net_device *stage09_ndev;

/* 【学习】stage09_now_ns() — 读取硬件 raw 时钟
 * ktime_get_ns() → ktime_get_raw_ns() 的 wrapper
 * 使用 raw 时钟是因为它不受 adjtimex 等时间调整影响，适合性能测量
 * 对比：ktime_get_real_ns() 会受系统时间调整干扰 */
static inline u64 stage09_now_ns(void)
{
    return ktime_get_ns();
}

/* 【学习】stage09_next_idx() — 环形 index 递增
 * (idx + 1) % size 实现环形buffer 的 wrap-around
 * 所有 ring index 都用这个宏更新，保证 0 ~ size-1 循环
 * 注意：取模操作在热路径上较慢，真实驱动会用 bitmask 优化（size 必须是 2^n）*/
static inline u16 stage09_next_idx(struct stage09_ring *r, u16 idx)
{
    return (idx + 1) % r->size;
}

/* 【学习】stage09_is_test_frame() — 识别教学测试帧
 * 条件 1：以太网类型 == 0x88B9（STAGE09_TEST_PROTO，教学专用）
 * 条件 2：skb 头部有 ETH_HLEN + 8 字节（存放 MAGIC + seq）
 * 条件 3：MAGIC 字符串匹配 "STAGE09"
 * 用途：在 stats 中区分测试帧和真实流量 */
static inline bool stage09_is_test_frame(struct sk_buff *skb)
{
    if (ntohs(skb->protocol) != STAGE09_TEST_PROTO)
        return false;
    if (skb_headlen(skb) < ETH_HLEN + 8)
        return false;
    return memcmp(skb->data + ETH_HLEN, STAGE09_TEST_MAGIC, 7) == 0;
}

/* 【学习】stage09_alloc_ring() — 分配 TX/RX ring 内存
 * kcalloc：零初始化的连续内存（k=kernel，calloc=零初始化）
 * GFP_KERNEL：允许睡眠的分配标志，可用于内存回收，是驱动最常用的标志
 * 与 GFP_ATOMIC 对比：GFP_ATOMIC 用于中断上下文，不允许睡眠
 *
 * 返回值：0 成功，-ENOMEM 内存不足
 * 注意：desc 和 slots 分别分配，任一失败都要释放已分配的那一个 */
static int stage09_alloc_ring(struct stage09_ring *r, u16 size)
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

/* 【学习】stage09_free_ring() — 释放 TX/RX ring 内存
 * 重要：在释放 ring 前，必须先释放所有关联的 skb 和 DMA 地址
 * DMA 地址不释放会导致内存泄漏（内核认为地址仍被使用）
 *
 * rx_ring 参数：区分 TX（DMA_TO_DEVICE）和 RX（DMA_FROM_DEVICE）映射方向
 * - true：RX ring，buffer 从设备来，用 DMA_FROM_DEVICE
 * - false：TX ring，buffer 发往设备，用 DMA_TO_DEVICE
 *
 * memset(r, 0, sizeof(*r))：将 ring 结构清零，防止悬空指针
 * dev_kfree_skb_any()：可从任意上下文调用的 skb 释放函数
 * 与 dev_kfree_skb() 对比：any 版本用于 softirq 等上下文 */
static void stage09_free_ring(struct net_device *ndev, struct stage09_ring *r, bool rx_ring)
{
    u16 i;
    if (!r->slots)
        goto out;
    for (i = 0; i < r->size; ++i) {
        struct stage09_buf_slot *s = &r->slots[i];
        if (s->skb) {
            dma_unmap_single(ndev->dev.parent ? ndev->dev.parent : &ndev->dev,
                             s->dma_addr, s->buf_len,
                             rx_ring ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
            dev_kfree_skb_any(s->skb);
        }
    }
out:
    kfree(r->slots);
    kfree(r->desc);
    memset(r, 0, sizeof(*r));
}

/* 【学习】stage09_reset_queue() — 重置队列状态
 * 用于设备 open 时或 close 后清理队列
 * 只清理 ring 内容和状态标志，不释放内存（内存由 alloc_ring/free_ring 管理）
 *
 * memset 清零 ring：所有 index 归 0，state 归 FREE
 * tx_inflight/tx_done/rx_posted/rx_ready 全部归 0：表示队列空闲
 * 注意：调用前 ring size 必须已设置（由 alloc_ring 分配） */
static void stage09_reset_queue(struct stage09_queue *q)
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

/* 【学习】stage09_post_rx_one() — 填充一个 RX buffer 到 ring
 * 由 stage09_refill_rx_all 调用，在设备 open 和 RX 消费时批量填充
 *
 * 返回值：0 成功，-ENOSPC ring 已满，-ENOMEM skb 分配失败，-EIO DMA 映射失败
 * rx_posted >= r->size - 1：保留一个 slot 防止完全填满（避免 RX 消费落后）
 *
 * DMA 映射：DMA_FROM_DEVICE 表示设备写入内存
 * netdev_alloc_skb vs dev_alloc_skb：
 * - netdev_alloc_skb：增加 NET_SKB_PAD，预留 headroom 用于协议头对齐
 * - dev_alloc_skb：更老旧的 API，现在推荐 netdev_alloc_skb */
static int stage09_post_rx_one(struct stage09_queue *q)
{
    struct stage09_priv *priv = q->priv;
    struct net_device *ndev = priv->ndev;
    struct stage09_ring *r = &q->rxq;
    struct stage09_desc *d;
    struct stage09_buf_slot *s;
    struct sk_buff *skb;
    dma_addr_t dma;
    u16 idx;

    if (q->rx_posted >= r->size - 1)
        return -ENOSPC;
    idx = r->post_idx;
    d = &r->desc[idx];
    s = &r->slots[idx];
    if (s->state != S09_SLOT_FREE)
        return -EBUSY;

    skb = netdev_alloc_skb(ndev, priv->rx_buf_size);
    if (!skb)
        return -ENOMEM;

    dma = dma_map_single(ndev->dev.parent ? ndev->dev.parent : &ndev->dev,
                         skb->data, priv->rx_buf_size, DMA_FROM_DEVICE);
    if (dma_mapping_error(ndev->dev.parent ? ndev->dev.parent : &ndev->dev, dma)) {
        dev_kfree_skb_any(skb);
        atomic64_inc(&q->stats.rx_dma_map_fail);
        return -EIO;
    }

    s->skb = skb;
    s->dma_addr = dma;
    s->buf_len = priv->rx_buf_size;
    s->data_len = 0;
    s->state = S09_SLOT_POSTED;
    s->id = idx;

    d->dma_addr = dma;
    d->data_len = priv->rx_buf_size;
    d->state = S09_SLOT_POSTED;

    r->post_idx = stage09_next_idx(r, r->post_idx);
    q->rx_posted++;
    atomic64_inc(&q->stats.rx_post_count);
    atomic64_inc(&q->stats.rx_dma_map_ok);
    return 0;
}

/* 【学习】stage09_refill_rx_all() — 批量填充 RX buffer
 * 在 stage09_open 中调用，将 RX ring 填满
 * 循环调用 stage09_post_rx_one() 直到 rx_posted 达到 ring size - 1
 *
 * 教学亮点：为什么要 size - 1？
 * - 如果 ring 完全填满，backend 的 device_idx == post_idx 时会误判为"已满"
 * - 保留一个 slot 留给大家做边界判断，类似于 TCP window 的"留一"设计 */
static void stage09_refill_rx_all(struct stage09_queue *q)
{
    while (q->rx_posted < q->rxq.size - 1) {
        if (stage09_post_rx_one(q))
            break;
    }
}

/* 【学习】stage09_raise_irq() — 触发软中断通知 NAPI 处理完成
 * 与 stage08 一样，通过 __napi_schedule() 将 NAPI 投入软中断
 *
 * 关键行为：
 * - irq_masked 为 false 时才触发（避免重复触发）
 * - napi_schedule_prep() 检查 NAPI 是否可以调度（已绑定、未在运行）
 * - __napi_schedule() 将 napi 移到当前 CPU 的 softirq 队列
 *
 * 教学亮点：为什么用 irq_masked？
 * - 当 NAPI poll 还没完成时，如果 backend 又处理完一批帧，不应该再触发 irq
 * - irq_masked 在 napi_complete_done() 时复位，用于"批量 irq"优化
 *
 * 对比真实硬件：virtio-net 用 PCI MSI-X 中断，这里用软件模拟 */
static void stage09_raise_irq(struct stage09_queue *q)
{
    q->timeline.last_irq_ns = stage09_now_ns();
    atomic64_inc(&q->stats.irq_count);
    if (!q->irq_masked && napi_schedule_prep(&q->napi)) {
        q->irq_masked = true;
        __napi_schedule(&q->napi);
    }
}

/* 【学习】stage09_mark_doorbell() — 敲 doorbell 通知 backend 有 TX 请求
 * 与 stage08 不同的是：stage09 每个队列独立调用自己的 doorbell
 *
 * doorbell_pending：标记"有事要处理，但还没处理完"
 * - 防止 backend 还没处理完时，新 work 又入队导致重复处理
 * - 防止 backend 处理完了，新入队的 work 被遗漏
 *
 * queue_work：将 backend_work item 加入 workqueue 调度队列
 * - 这是一个"通知"机制，不是"等待"机制
 * - 如果 workqueue 线程正忙，work 会在队列里等待
 *
 * 【学习】为什么 queue_work 前要检查 doorbell_pending？
 * - 如果上一次 doorbell_pending==true（还未处理完），不需要重复入队
 * - 节省 workqueue 开销，backend 处理完会重新检查并可能重新入队 */
static void stage09_mark_doorbell(struct stage09_queue *q)
{
    q->timeline.last_doorbell_ns = stage09_now_ns();
    atomic64_inc(&q->stats.doorbell_count);
    atomic64_inc(&q->stats.backend_schedule_count);
    if (!q->doorbell_pending) {
        q->doorbell_pending = true;
        queue_work(q->priv->backend_wq, &q->backend_work);
    }
}

/* 【学习】stage09_complete_tx_one() — NAPI 回收一个 TX 完成帧
 * 从 complete_idx 位置取出 DONE slot，释放 DMA 和 skb
 *
 * 前置条件：
 * - tx_done > 0（有待回收）
 * - slot.state == S09_SLOT_DONE（backend 已处理完）
 *
 * 生命周期管理：
 * 1. dma_unmap_single：释放 DMA 地址（TX 用 DMA_TO_DEVICE）
 * 2. dev_consume_skb_any：释放 skb（any 版本可在 softirq 上下文调用）
 * 3. memset 清零 slot/desc（防止悬空指针）
 * 4. complete_idx++，tx_done--，tx_inflight--
 * 5. 更新 timeline.last_complete_ns
 *
 * 与 stage08 的 complete_tx_one 完全一致，是多队列版本 */
static int stage09_complete_tx_one(struct stage09_queue *q)
{
    struct net_device *ndev = q->priv->ndev;
    struct stage09_ring *r = &q->txq;
    struct stage09_desc *d = &r->desc[r->complete_idx];
    struct stage09_buf_slot *s = &r->slots[r->complete_idx];

    if (!q->tx_done || s->state != S09_SLOT_DONE)
        return 0;

    dma_unmap_single(ndev->dev.parent ? ndev->dev.parent : &ndev->dev,
                     s->dma_addr, d->data_len, DMA_TO_DEVICE);
    atomic64_inc(&q->stats.tx_dma_unmap);
    if (s->skb) {
        dev_consume_skb_any(s->skb);
        s->skb = NULL;
    }
    memset(s, 0, sizeof(*s));
    memset(d, 0, sizeof(*d));
    r->complete_idx = stage09_next_idx(r, r->complete_idx);
    q->tx_done--;
    q->tx_inflight--;
    q->timeline.last_complete_ns = stage09_now_ns();
    atomic64_inc(&q->stats.tx_complete_count);
    return 1;
}

/* 【学习】stage09_consume_rx_one() — NAPI 消费一个完成的 RX 帧
 * 从 consume_idx 位置取出 DONE slot，上送协议栈并补充 RX buffer
 *
 * 前置条件：
 * - rx_ready > 0（有待消费）
 * - slot.state == S09_SLOT_DONE（backend 已生产数据）
 *
 * 关键步骤：
 * 1. skb_trim(skb, 0)：重置 skb 长度到 0（清除 headroom）
 * 2. skb_put(skb, len)：设置正确的数据长度
 * 3. eth_type_trans()：识别上层协议，设置 skb->protocol
 * 4. netif_receive_skb()：上送协议栈（替代 netif_rx，更高效）
 * 5. memset 清零 slot/desc
 * 6. stage09_post_rx_one()：立即补充空 buffer（refill）
 *
 * skb_trim + skb_put 组合：
 * - skb_trim 清除协议头前的一切（truesize 保留）
 * - skb_put 在 tail 扩展数据长度
 * - 相当于把 buffer 从"allocated size" 重置为"actual data size" */
static int stage09_consume_rx_one(struct stage09_queue *q)
{
    struct stage09_buf_slot saved = { 0 };
    struct stage09_ring *r = &q->rxq;
    struct stage09_desc *d;
    struct stage09_buf_slot *s;
    u16 idx;
    __be16 proto;

    if (!q->rx_ready)
        return 0;

    idx = r->consume_idx;
    d = &r->desc[idx];
    s = &r->slots[idx];
    if (d->state != S09_SLOT_DONE || s->state != S09_SLOT_DONE || !s->skb)
        return 0;

    saved = *s;
    memset(s, 0, sizeof(*s));
    s->id = idx;
    s->state = S09_SLOT_FREE;
    memset(d, 0, sizeof(*d));
    d->state = S09_SLOT_FREE;
    r->consume_idx = stage09_next_idx(r, r->consume_idx);
    if (q->rx_ready > 0)
        q->rx_ready--;
    q->timeline.last_consume_ns = stage09_now_ns();

    dma_unmap_single(q->priv->ndev->dev.parent ? q->priv->ndev->dev.parent : &q->priv->ndev->dev,
                     saved.dma_addr, saved.buf_len, DMA_FROM_DEVICE);
    atomic64_inc(&q->stats.rx_dma_unmap);

    skb_put(saved.skb, saved.data_len);
    proto = eth_type_trans(saved.skb, q->priv->ndev);
    if (proto == htons(STAGE09_TEST_PROTO))
        atomic64_inc(&q->stats.test_rx_consume_count);
    netif_receive_skb(saved.skb);

    atomic64_inc(&q->stats.rx_consume_count);
    atomic64_inc(&q->stats.rx_packets);
    atomic64_add(saved.data_len, &q->stats.rx_bytes);

    if (stage09_post_rx_one(q))
        atomic64_inc(&q->stats.rx_dropped);
    return 1;
}

/* 【学习】stage09_backend_workfn() — backend 异步处理函数（workqueue bottom-half）
 * 与 stage08 的 backend_workfn 完全一致，是 per-queue 版本
 *
 * 上下文：workqueue 线程（可能是任意 CPU）
 * 持有锁：priv->state_lock
 *
 * 关键行为：
 * 1. usleep_range(backend_delay_us)：可选延迟，模拟真实设备处理时间
 * 2. 处理 TX（notify_idx → DONE）和 RX（POSTED → DONE）配对
 * 3. memcpy(TX_data → RX_skb)：模拟 DMA 复制（教学简化版）
 * 4. 处理完一批后 raise_irq 通知 NAPI
 * 5. 如果还有未处理帧，need_resched 触发重新入队
 *
 * 双层循环设计：
 * - 外层：限制 batch（backend_batch），避免单次处理过长
 * - 内层：检查 TX slot 有数据和 RX slot 有空位，才处理
 *
 * doorbell_pending 重入逻辑：
 * - 处理完当前批后发现 txq.notify_idx != txq.submit_idx，说明还有未处理 TX
 * - 置 doorbell_pending=true，下次 ndo_start_xmit 或 NAPI complete 时可能重新入队
 *
 * 【学习】为什么要 backend_running 标志？
 * - 如果 backend 还没处理完（backend_running=true）时，新 doorbell 又来了
 * - doorbell_pending 仍为 true，但 backend 正在执行，不需要再次入队
 * - 避免重复入队开销，backend 完成后会自己检查并重入 */
static void stage09_backend_workfn(struct work_struct *work)
{
    struct stage09_queue *q = container_of(work, struct stage09_queue, backend_work);
    struct stage09_priv *priv = q->priv;
    unsigned long flags;
    int processed = 0;
    bool need_resched = false;

    if (priv->backend_delay_us)
        usleep_range(priv->backend_delay_us, priv->backend_delay_us + 50);

    spin_lock_irqsave(&priv->state_lock, flags);
    q->doorbell_pending = false;
    q->backend_running = true;
    q->timeline.last_backend_wakeup_ns = stage09_now_ns();
    atomic64_inc(&q->stats.backend_run_count);

    while (processed < priv->backend_batch) {
        struct stage09_ring *txr = &q->txq;
        struct stage09_ring *rxr = &q->rxq;
        struct stage09_desc *txd, *rxd;
        struct stage09_buf_slot *txs, *rxs;
        u16 txi, rxi;
        u32 copy_len;

        /* 无 TX 待处理或有 RX 空位，退出 */
        if (txr->notify_idx == txr->submit_idx)
            break;
        if (!q->rx_posted)
            break;

        txi = txr->notify_idx;
        rxi = rxr->device_idx;
        txd = &txr->desc[txi];
        txs = &txr->slots[txi];
        rxd = &rxr->desc[rxi];
        rxs = &rxr->slots[rxi];

        /* 状态检查，防止重复处理 */
        if (txs->state != S09_SLOT_SUBMITTED)
            break;
        if (rxs->state != S09_SLOT_POSTED)
            break;

        /* 【学习】TX→RX 数据通路（loopback 教学模型）
         * memcpy 模拟 DMA transfer（真实 virtio-net 用 DMA scatter-gather）
         * min(txd->data_len, rxs->buf_len) 防止 buffer overflow
         * 真实驱动会检查 MTU，这里简化处理 */
        copy_len = min(txd->data_len, rxs->buf_len);
        skb_put(rxs->skb, copy_len);
        memcpy(rxs->skb->data, txs->skb->data, copy_len);
        rxs->data_len = copy_len;
        rxs->state = S09_SLOT_DONE;
        rxd->data_len = copy_len;
        rxd->state = S09_SLOT_DONE;
        rxr->device_idx = stage09_next_idx(rxr, rxr->device_idx);
        q->rx_posted--;
        q->rx_ready++;

        txs->state = S09_SLOT_DONE;
        txd->state = S09_SLOT_DONE;
        txr->notify_idx = stage09_next_idx(txr, txr->notify_idx);
        q->tx_done++;

        atomic64_inc(&q->stats.backend_tx_processed);
        atomic64_inc(&q->stats.backend_rx_produced);
        processed++;
    }

    q->timeline.last_backend_done_ns = stage09_now_ns();
    if (processed)
        stage09_raise_irq(q);
    if (q->txq.notify_idx != q->txq.submit_idx)
        need_resched = true;
    q->backend_running = false;
    spin_unlock_irqrestore(&priv->state_lock, flags);

    if (need_resched)
        stage09_mark_doorbell(q);
}

static int stage09_napi_poll(struct napi_struct *napi, int budget)
/* 【学习】stage09_napi_poll() — NAPI 轮询函数（per-queue）
 * 与 stage08 的 poll 函数一致，是 per-queue 版本
 *
 * 上下文：softirq（软中断）
 * 持有锁：priv->state_lock
 *
 * budget 机制：
 * - 内核传入 budget，限制每次 poll 最多处理多少帧
 * - 防止 poll 函数霸占 CPU 太长时间，保证实时性
 * - 返回实际处理的工作量（work_done）
 *
 * 两层 while 循环：
 * 1. 先回收 TX done（complete_tx_one），清空 tx_done
 * 2. 再消费 RX ready（consume_rx_one），受 budget 限制
 *
 * napi_complete_done 条件：
 * - rx_ready==0（RX 全部处理完）
 * - tx_done==0（TX 全部回收完）
 * - 如果还有待处理 work，重新 mark_doorbell（防止漏处理） */
{
    struct stage09_queue *q = container_of(napi, struct stage09_queue, napi);
    struct stage09_priv *priv = q->priv;
    int work_done = 0;
    unsigned long flags;

    atomic64_inc(&q->stats.napi_poll_count);
    spin_lock_irqsave(&priv->state_lock, flags);
    q->timeline.last_poll_ns = stage09_now_ns();

    while (stage09_complete_tx_one(q))
        ;

    while (work_done < budget && stage09_consume_rx_one(q))
        work_done++;

    atomic64_add(work_done, &q->stats.napi_work_total);

    if (work_done < budget && q->rx_ready == 0) {
        napi_complete_done(napi, work_done);
        q->irq_masked = false;
        atomic64_inc(&q->stats.napi_complete_count);
        if (q->doorbell_pending || q->txq.notify_idx != q->txq.submit_idx)
            stage09_mark_doorbell(q);
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);
    return work_done;
}

static netdev_tx_t stage09_start_xmit(struct sk_buff *skb, struct net_device *ndev)
/* 【学习】stage09_start_xmit() — TX 发送函数（多队列版本）
 * 与 stage08 的 xmit 类似，但选择了目标队列 qid
 *
 * 关键多队列逻辑：
 * qid = skb_get_queue_mapping(skb) % priv->num_queues
 * - skb_get_queue_mapping() 返回内核为这个 skb 分配的队列号
 * - 内核在 earlier 层面（IP 层或 socket 层）根据 flow hash 分配队列
 * - % num_queues 确保队列号在有效范围内
 *
 * 流程：
 * 1. 选择目标队列（qid）
 * 2. skb_linearize（非线性 skb 才需要，合并碎片）
 * 3. 检查 tx_inflight >= ring_size-1（防止 overflow）
 * 4. DMA map skb->data
 * 5. 分配 slot，更新 state=SUBMITTED
 * 6. submit_idx++，tx_inflight++
 * 7. 更新 timeline，计数
 * 8. stage09_mark_doorbell() 触发 backend 处理
 *
 * 返回值：
 * - NETDEV_TX_OK：成功入队（不等于已发送）
 * - NETDEV_TX_BUSY：ring full，应该重试
 * - 注意：即使返回 OK，帧也在队列里，不保证最终到达 */
{
    struct stage09_priv *priv = netdev_priv(ndev);
    struct stage09_queue *q;
    struct stage09_ring *r;
    struct stage09_desc *d;
    struct stage09_buf_slot *s;
    unsigned long flags;
    dma_addr_t dma;
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
    if (s->state != S09_SLOT_FREE) {
        atomic64_inc(&q->stats.tx_busy);
        spin_unlock_irqrestore(&priv->state_lock, flags);
        return NETDEV_TX_BUSY;
    }

    dma = dma_map_single(ndev->dev.parent ? ndev->dev.parent : &ndev->dev,
                         skb->data, skb_headlen(skb), DMA_TO_DEVICE);
    if (dma_mapping_error(ndev->dev.parent ? ndev->dev.parent : &ndev->dev, dma)) {
        atomic64_inc(&q->stats.tx_dma_map_fail);
        atomic64_inc(&q->stats.tx_dropped);
        spin_unlock_irqrestore(&priv->state_lock, flags);
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }

    s->skb = skb;
    s->dma_addr = dma;
    s->buf_len = skb_headlen(skb);
    s->data_len = skb_headlen(skb);
    s->state = S09_SLOT_SUBMITTED;
    s->id = idx;
    d->dma_addr = dma;
    d->data_len = skb_headlen(skb);
    d->state = S09_SLOT_SUBMITTED;
    r->submit_idx = stage09_next_idx(r, r->submit_idx);
    q->tx_inflight++;

    q->timeline.last_submit_ns = stage09_now_ns();
    atomic64_inc(&q->stats.tx_submit_count);
    atomic64_inc(&q->stats.tx_packets);
    atomic64_add(skb_headlen(skb), &q->stats.tx_bytes);
    atomic64_inc(&q->stats.tx_dma_map_ok);
    if (stage09_is_test_frame(skb))
        atomic64_inc(&q->stats.test_tx_submit_count);

    stage09_mark_doorbell(q);
    spin_unlock_irqrestore(&priv->state_lock, flags);
    return NETDEV_TX_OK;
}

/* 【学习】stage09_select_queue() — 队列选择回调（多队列分发策略）
 * 内核在发送前会调用这个函数决定用哪个队列
 * 这是 ndo_select_queue 回调的实现（ndo_ 开头的都是 netdev operations）
 *
 * 分发策略：
 * 1. 如果 skb 有 hash（skb_get_hash() != 0）：使用 hash % num_queues
 *    - hash 通常来自 5-tuple（src_ip, dst_ip, src_port, dst_port, protocol）
 *    - 保证同一 flow 的帧到同一个队列（保序）
 *    - reciprocal_scale 是比 % 更快的除法（针对 2^n 的优化）
 * 2. 如果没有 hash：用 round-robin（rr_counter 原子递增 % num_queues）
 *    - 保证无 hash 流量也能分散到各队列
 *
 * 教学亮点：为什么 hash 优先？
 * - 网络流通常有多个帧，同一 flow 到同一队列保证保序
 * - CPU cache 友好（同一 flow 的数据在同一 CPU 处理）
 * - 真实 NIC RSS（Receive Side Scaling）也是类似原理 */
static u16 stage09_select_queue(struct net_device *ndev, struct sk_buff *skb,
                                struct net_device *sb_dev)
{
    struct stage09_priv *priv = netdev_priv(ndev);
    u32 hash = skb_get_hash(skb);

    if (hash)
        return reciprocal_scale(hash, priv->num_queues);
    return atomic64_inc_return(&priv->rr_counter) % priv->num_queues;
}

/* 【学习】stage09_open() — 设备 UP 时调用
 * 等同于 ifconfig nds9 up
 *
 * 关键步骤：
 * 1. 重置所有队列状态（stage09_reset_queue）
 * 2. 填充所有 RX buffer（stage09_refill_rx_all）
 * 3. 使能所有 NAPI（napi_enable）
 * 4. 启动所有 TX 队列（netif_tx_start_all_queues）
 *
 * napi_enable vs napi_disable：
 * - enable：允许 NAPI 被调度到 softirq
 * - disable：禁止调度，必须配对使用
 *
 * netif_tx_start_all_queues：
 * - 启动所有 TX 队列，允许帧发送
 * - 对应 netif_tx_disable（stop 时调用） */
static int stage09_open(struct net_device *ndev)
{
    struct stage09_priv *priv = netdev_priv(ndev);
    unsigned long flags;
    int i;

    spin_lock_irqsave(&priv->state_lock, flags);
    for (i = 0; i < priv->num_queues; ++i) {
        stage09_reset_queue(&priv->queues[i]);
        stage09_refill_rx_all(&priv->queues[i]);
        napi_enable(&priv->queues[i].napi);
    }
    atomic64_inc(&priv->open_count);
    spin_unlock_irqrestore(&priv->state_lock, flags);

    netif_tx_start_all_queues(ndev);
    return 0;
}

/* 【学习】stage09_stop() — 设备 DOWN 时调用
 * 等同于 ifconfig nds9 down
 *
 * 关键步骤：
 * 1. 停止所有 TX 队列（netif_tx_disable）
 * 2. 刷新 backend workqueue（flush_workqueue，等待所有 work 执行完）
 * 3. 禁止所有 NAPI（napi_disable）
 *
 * flush_workqueue 的必要性：
 * - rmmod 前必须确保没有 work 还在运行
 * - 否则可能导致 use-after-free
 * - 真实驱动在 remove 时也要 flush all pending work */
static int stage09_stop(struct net_device *ndev)
{
    struct stage09_priv *priv = netdev_priv(ndev);
    int i;

    netif_tx_disable(ndev);
    if (priv->backend_wq)
        flush_workqueue(priv->backend_wq);
    for (i = 0; i < priv->num_queues; ++i)
        napi_disable(&priv->queues[i].napi);
    atomic64_inc(&priv->stop_count);
    return 0;
}

/* 【学习】stage09_get_stats64() — 获取设备统计（ethtool -S）
 * ndo_get_stats64 是内核获取设备统计的回调
 *
 * 实现方式：遍历所有队列，累加各队列的统计
 * - 每队列 stats 是 atomic64_read，读是原子的
 * - 累加过程不需要锁（读是原子的，但不保证读到他的一致性快照）
 *
 * rtnl_link_stats64 结构：
 * 内核标准统计结构，包含 tx_packets/rx_packets/tx_bytes/rx_bytes 等
 * ethtool -S 和 ip -s link show 都用这个回调 */
static void stage09_get_stats64(struct net_device *ndev, struct rtnl_link_stats64 *stats)
{
    struct stage09_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage09_queue *q = &priv->queues[i];
        stats->tx_packets += atomic64_read(&q->stats.tx_packets);
        stats->tx_bytes += atomic64_read(&q->stats.tx_bytes);
        stats->tx_dropped += atomic64_read(&q->stats.tx_dropped);
        stats->rx_packets += atomic64_read(&q->stats.rx_packets);
        stats->rx_bytes += atomic64_read(&q->stats.rx_bytes);
        stats->rx_dropped += atomic64_read(&q->stats.rx_dropped);
    }
}

static const struct net_device_ops stage09_netdev_ops = {
/* 【学习】net_device_ops — 网络设备的操作函数表
 * 类似 file_operations，内核用这个结构找到设备支持的操作
 * stage09 支持 5 个 ndo_ 回调：
 * - ndo_open：ifconfig up 时调用
 * - ndo_stop：ifconfig down 时调用
 * - ndo_start_xmit：发送帧时调用（TX 路径起点）
 * - ndo_select_queue：选择用哪个队列发送（多队列分发）
 * - ndo_get_stats64：获取设备统计（ethtool -S）
 *
 * 与 stage08 对比：
 * stage08 没有 ndo_select_queue（单队列不需要）
 * stage09 有 ndo_select_queue 是因为多队列需要分发 */
    .ndo_open = stage09_open,
    .ndo_stop = stage09_stop,
    .ndo_start_xmit = stage09_start_xmit,
    .ndo_select_queue = stage09_select_queue,
    .ndo_get_stats64 = stage09_get_stats64,
};

/* 【学习】stage09_stats_show() — debugfs stats 文件读取函数
 * seq_file 接口：内核用于生成有序输出的文件接口
 * 替代简单 seq_printf 直接输出，更适合多行/复杂格式
 *
 * seq_file 使用模式：
 * 1. 实现 show() 回调：用 seq_printf 输出每一行
 * 2. 实现 open() 回调：用 single_open 包装
 * 3. 定义 file_operations：用 seq_read/seq_lseek/single_release
 *
 * 输出格式：每行 key=value，便于脚本解析
 * cat /sys/kernel/debug/netdev_stage09/stats 可以看到所有队列统计 */
static int stage09_stats_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage09_priv *priv = netdev_priv(ndev);
    int i;

    seq_printf(m, "ifname=%s num_queues=%u ring_size=%u napi_weight=%u backend_batch=%u open_count=%lld stop_count=%lld\n",
               ndev->name, priv->num_queues, priv->ring_size, priv->napi_weight,
               priv->backend_batch, atomic64_read(&priv->open_count), atomic64_read(&priv->stop_count));
    for (i = 0; i < priv->num_queues; ++i) {
        struct stage09_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx_submit=%lld tx_complete=%lld tx_packets=%lld tx_bytes=%lld tx_busy=%lld tx_drop=%lld rx_post=%lld rx_consume=%lld rx_packets=%lld rx_bytes=%lld rx_drop=%lld doorbell=%lld backend_schedule=%lld backend_run=%lld backend_tx=%lld backend_rx=%lld irq=%lld napi_poll=%lld napi_complete=%lld napi_work=%lld test_tx=%lld test_rx=%lld\n",
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
                   atomic64_read(&q->stats.backend_schedule_count),
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

static int stage09_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage09_stats_show, inode->i_private);
}

static const struct file_operations stage09_stats_fops = {
    .owner = THIS_MODULE,
    .open = stage09_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/* 【学习】stage09_queues_show() — debugfs queues 文件读取
 * 显示每个队列的实时 ring index 和 slot 状态
 *
 * 每队列输出：
 * - 6 个 TX index：submit/notify/complete/inflight/done
 * - 6 个 RX index：post/device/consume/posted/ready
 * - 3 个状态标志：irq_masked/doorbell_pending/backend_running
 * - 前 8 个 TX slot 状态（前 8 个 RX slot 状态）
 *
 * STAGE09_QUEUE_DUMP_LIMIT=8：限制打印 slot 数，防止输出过长
 * min_t(u16, size, 8)：取较小值，确保不越界 */
static int stage09_queues_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage09_priv *priv = netdev_priv(ndev);
    int i, j;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage09_queue *q = &priv->queues[i];
        seq_printf(m,
                   "q%u: tx submit=%u notify=%u complete=%u inflight=%u done=%u | rx post=%u device=%u consume=%u posted=%u ready=%u irq_masked=%u doorbell_pending=%u backend_running=%u\n",
                   q->qid, q->txq.submit_idx, q->txq.notify_idx, q->txq.complete_idx,
                   q->tx_inflight, q->tx_done, q->rxq.post_idx, q->rxq.device_idx,
                   q->rxq.consume_idx, q->rx_posted, q->rx_ready,
                   q->irq_masked, q->doorbell_pending, q->backend_running);
        for (j = 0; j < min_t(u16, q->txq.size, STAGE09_QUEUE_DUMP_LIMIT); ++j)
            seq_printf(m, "  q%u txslot[%d]: state=%u len=%u\n", q->qid, j,
                       q->txq.slots[j].state, q->txq.slots[j].data_len);
        for (j = 0; j < min_t(u16, q->rxq.size, STAGE09_QUEUE_DUMP_LIMIT); ++j)
            seq_printf(m, "  q%u rxslot[%d]: state=%u len=%u last_seq=%u\n", q->qid, j,
                       q->rxq.slots[j].state, q->rxq.slots[j].data_len, q->rxq.slots[j].last_seq);
    }
    return 0;
}

/* 【学习】stage09_timeline_show() — debugfs timeline 文件读取
 * 显示每个队列最近一次完整 TX→RX 事务的 8 个时间戳和 4 个 delta
 *
 * delta 计算：
 * - d1 = doorbell - submit（submit 到 doorbell 的延迟）
 * - d2 = backend_wakeup - doorbell（doorbell 到 backend 执行的延迟，核心异步指标）
 * - d3 = irq - backend_done（backend 处理完到 irq 的延迟）
 * - d4 = poll - irq（irq 到 NAPI poll 的延迟）
 *
 * d2 > 0 是证明"异步"的核心：说明 backend 是被 schedule 的，不是同步调用的
 *
 * 教学亮点：为什么 timeline 是 per-queue 的？
 * - 多队列下，每个队列的异步延迟可能不同
 * - 通过 per-queue timeline 可以观测队列间负载是否均衡 */

static int stage09_queues_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage09_queues_show, inode->i_private);
}

static const struct file_operations stage09_queues_fops = {
    .owner = THIS_MODULE,
    .open = stage09_queues_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage09_timeline_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage09_priv *priv = netdev_priv(ndev);
    int i;

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage09_queue *q = &priv->queues[i];
        u64 d1 = q->timeline.last_doorbell_ns > q->timeline.last_submit_ns ?
                 q->timeline.last_doorbell_ns - q->timeline.last_submit_ns : 0;
        u64 d2 = q->timeline.last_backend_wakeup_ns > q->timeline.last_doorbell_ns ?
                 q->timeline.last_backend_wakeup_ns - q->timeline.last_doorbell_ns : 0;
        u64 d3 = q->timeline.last_irq_ns > q->timeline.last_backend_done_ns ?
                 q->timeline.last_irq_ns - q->timeline.last_backend_done_ns : 0;
        u64 d4 = q->timeline.last_poll_ns > q->timeline.last_irq_ns ?
                 q->timeline.last_poll_ns - q->timeline.last_irq_ns : 0;
        seq_printf(m,
                   "q%u: submit_ns=%llu doorbell_ns=%llu backend_wakeup_ns=%llu backend_done_ns=%llu irq_ns=%llu poll_ns=%llu complete_ns=%llu consume_ns=%llu submit_to_doorbell_ns=%llu doorbell_to_backend_ns=%llu backend_to_irq_ns=%llu irq_to_poll_ns=%llu\n",
                   q->qid,
                   q->timeline.last_submit_ns, q->timeline.last_doorbell_ns,
                   q->timeline.last_backend_wakeup_ns, q->timeline.last_backend_done_ns,
                   q->timeline.last_irq_ns, q->timeline.last_poll_ns,
                   q->timeline.last_complete_ns, q->timeline.last_consume_ns,
                   d1, d2, d3, d4);
    }
    return 0;
}

static int stage09_timeline_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage09_timeline_show, inode->i_private);
}

static const struct file_operations stage09_timeline_fops = {
    .owner = THIS_MODULE,
    .open = stage09_timeline_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static void stage09_debugfs_init(struct stage09_priv *priv)
/* 【学习】stage09_debugfs_init() — 创建 debugfs 目录和文件
 * debugfs：内核提供的调试文件系统，挂载在 /sys/kernel/debug/
 * debugfs_create_dir() 创建子目录，debugfs_create_file() 创建文件
 *
 * 创建的文件：
 * - stats：设备统计（每队列 tx/rx/doorbell/backend/irq 等计数器）
 * - queues：每个队列的 ring index 和 slot 状态
 * - timeline：每个队列最近一次事务的 8 个时间戳和 4 个 delta
 *
 * 权限 0444：只读（所有用户可读，root 写）
 * 传入 priv->ndev 作为 seq_file 的 private 指针，用于 show 回调访问 netdev
 *
 * debugfs_remove_recursive()：递归删除整个目录及其下所有文件
 * 在 exit 时调用，防止卸载后目录残留 */
{
    priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
    if (!priv->dbg_dir)
        return;
    debugfs_create_file("stats", 0444, priv->dbg_dir, priv->ndev, &stage09_stats_fops);
    debugfs_create_file("queues", 0444, priv->dbg_dir, priv->ndev, &stage09_queues_fops);
    debugfs_create_file("timeline", 0444, priv->dbg_dir, priv->ndev, &stage09_timeline_fops);
}

static void stage09_debugfs_deinit(struct stage09_priv *priv)
{
    debugfs_remove_recursive(priv->dbg_dir);
    priv->dbg_dir = NULL;
}

/* 【学习】stage09_init() — 模块初始化（insmod 入口）
 * __init：编译器提示这个函数只调用一次，之后可以释放内存（不再需要）
 * __exit：同理，模块卸载时可以用空函数替代
 *
 * 完整初始化流程（6 步）：
 * 1. 参数校验 + clamp：num_queues/ring_size/napi_weight/backend_batch 合法性检查
 * 2. alloc_etherdev_mqs：分配 net_device 和 priv，设置队列数
 * 3. 初始化 priv：设置参数、初始化 spinlock
 * 4. 创建 backend_workqueue：WQ_UNBOUND（不绑定 CPU）+ WQ_MEM_RECLAIM（内存回收）
 * 5. 初始化每个队列：分配 ring、注册 NAPI、reset 状态
 * 6. register_netdev：向内核注册设备，nds9 出现在 ifconfig
 *
 * 关键对比：alloc_etherdev_mqs vs alloc_netdev + ether_setup
 * - alloc_etherdev_mqs：一步到位，自动设置 ETH_HLEN/addr_len/type，设定 TX/RX 队列数
 * - alloc_netdev + ether_setup：手动版，更灵活但需要更多代码
 *
 * err 标号：错误处理路径，跳转到这里清理已分配资源
 * 注意：错误处理严格按照分配顺序反向释放（ring → workqueue → netdev） */
static int __init stage09_init(void)
{
    struct net_device *ndev;
    struct stage09_priv *priv;
    int i, ret;

    num_queues = clamp_t(unsigned int, num_queues, 1, STAGE09_MAX_QUEUES);
    ring_size = max_t(unsigned int, ring_size, 32);
    napi_weight = max_t(unsigned int, napi_weight, 16);
    backend_batch = max_t(unsigned int, backend_batch, 1);

    ndev = alloc_etherdev_mqs(sizeof(struct stage09_priv), num_queues, num_queues);
    if (!ndev)
        return -ENOMEM;

    strscpy(ndev->name, ifname, IFNAMSIZ);
    ndev->netdev_ops = &stage09_netdev_ops;
    ndev->needs_free_netdev = true;
    eth_hw_addr_random(ndev);

    /* 【学习】DMA 能力准备
     * 在进行 DMA 映射之前，必须设置 dma_mask
     * alloc_etherdev_mqs 不会自动设置，需要手动设置
     * 失败时回退到 32-bit DMA */
    ndev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
    ndev->dev.dma_mask = &ndev->dev.coherent_dma_mask;
    if (dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(64))) {
        ret = dma_set_mask_and_coherent(&ndev->dev, DMA_BIT_MASK(32));
        if (ret) {
            pr_warn(DRV_NAME ": dma_set_mask_and_coherent failed: %d\n", ret);
            free_netdev(ndev);
            return ret;
        }
    }

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

    priv->backend_wq = alloc_workqueue("stage09_backend", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
    if (!priv->backend_wq) {
        free_netdev(ndev);
        return -ENOMEM;
    }

    for (i = 0; i < priv->num_queues; ++i) {
        struct stage09_queue *q = &priv->queues[i];
        q->priv = priv;
        q->qid = i;
        INIT_WORK(&q->backend_work, stage09_backend_workfn);
        ret = stage09_alloc_ring(&q->txq, ring_size);
        if (ret)
            goto err;
        ret = stage09_alloc_ring(&q->rxq, ring_size);
        if (ret)
            goto err;
        STAGE09_NETIF_NAPI_ADD(ndev, &q->napi, stage09_napi_poll, napi_weight);
        stage09_reset_queue(q);
    }

    ret = register_netdev(ndev);
    if (ret)
        goto err;

    stage09_debugfs_init(priv);
    stage09_ndev = ndev;
    pr_info(DRV_NAME ": loaded ifname=%s num_queues=%u ring_size=%u napi_weight=%u backend_batch=%u\n",
            ndev->name, priv->num_queues, priv->ring_size, priv->napi_weight, priv->backend_batch);
    return 0;
err:
    for (i = 0; i < priv->num_queues; ++i) {
        if (priv->queues[i].napi.dev)
            netif_napi_del(&priv->queues[i].napi);
        stage09_free_ring(ndev, &priv->queues[i].txq, false);
        stage09_free_ring(ndev, &priv->queues[i].rxq, true);
    }
    destroy_workqueue(priv->backend_wq);
    free_netdev(ndev);
    return ret;
}

static void __exit stage09_exit(void)
/* 【学习】stage09_exit() — 模块卸载入口（rmmod 出口）
 * __exit：同理，模块卸载时可以用空函数替代（如果没编译成内置驱动）
 *
 * 完整卸载流程（反向顺序，4 步）：
 * 1. stage09_debugfs_deinit：删除 debugfs 目录
 * 2. unregister_netdev：从内核移除 netdev（ifconfig 看不到 nds9 了）
 * 3. flush_workqueue + destroy_workqueue：等待所有 backend work 执行完毕
 * 4. 每个队列：cancel_work_sync（取消 work）+ netif_napi_del + free_ring
 * 5. free_netdev：释放 net_device 和 priv 内存
 *
 * 关键逆向保证：
 * - 谁分配谁释放：alloc → free，register → unregister
 * - 顺序反向：后分配先释放
 * - 每个队列单独取消：cancel_work_sync 确保 work 函数不会在模块卸载后运行
 *
 * cancel_work_sync vs flush_workqueue：
 * - cancel_work_sync：取消 work 并等待其执行完毕（如果正在运行则阻塞至完成）
 * - flush_workqueue：等待队列中所有 work 执行完毕（不取消）
 * - 这里用 cancel_work_sync 防止重新入队 */
{
    struct stage09_priv *priv;
    int i;

    if (!stage09_ndev)
        return;
    priv = netdev_priv(stage09_ndev);
    stage09_debugfs_deinit(priv);
    unregister_netdev(stage09_ndev);
    flush_workqueue(priv->backend_wq);
    destroy_workqueue(priv->backend_wq);
    for (i = 0; i < priv->num_queues; ++i) {
        cancel_work_sync(&priv->queues[i].backend_work);
        netif_napi_del(&priv->queues[i].napi);
        stage09_free_ring(stage09_ndev, &priv->queues[i].txq, false);
        stage09_free_ring(stage09_ndev, &priv->queues[i].rxq, true);
    }
    free_netdev(stage09_ndev);
    stage09_ndev = NULL;
    pr_info(DRV_NAME ": unloaded\n");
}

module_init(stage09_init);
module_exit(stage09_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("stage09 multi queue scaling teaching netdev");
