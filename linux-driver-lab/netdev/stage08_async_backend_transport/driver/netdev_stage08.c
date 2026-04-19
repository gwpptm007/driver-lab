// SPDX-License-Identifier: GPL-2.0
/*
 * netdev_stage08.c — stage08_async_backend_transport 教学驱动
 *
 * 【stage08 核心目标】
 *
 * 在 stage07 基础上真正实现前后端分离：
 * - Front-end（前端）：协议栈侧，负责 submit、doorbell、NAPI poll
 * - Back-end（后端）：独立 workqueue，负责异步处理 TX、产生 RX、触发 IRQ
 *
 * 【新增关键概念】
 *
 * 1. doorbell_pending：标记是否有待处理的工作，避免重复调度
 * 2. backend_work：独立 work_struct，在独立 CPU 上异步执行
 * 3. timeline 观测：精确记录各阶段的时间戳，用于分析异步延迟
 *
 * 【TX 路径】
 *   ndo_start_xmit()
 *     -> dma_map_single()         【学习】DMA 映射 skb->data
 *     -> slot[submit_idx] = SUBMITTED
 *     -> submit_idx++
 *     -> stage08_mark_doorbell()   【学习】触发后端
 *       -> doorbell_pending = true
 *       -> queue_work(backend_wq, backend_work)
 *
 * 【Backend Work 路径】
 *   backend_workfn()
 *     -> 处理 slot[notify_idx..submit_idx-1] 的 TX 帧
 *     -> 复制到对应 RX slot
 *     -> slot[device_idx] = DONE
 *     -> device_idx++
 *     -> raise IRQ
 *
 * 【RX 路径（中断后）】
 *   napi_schedule() -> napi_poll()
 *     -> consume_rx_one()         【学习】把 RX slot 交给协议栈
 *       -> netif_receive_skb()
 *     -> refill_one()             【学习】补充 RX buffer
 *       -> netdev_alloc_skb()
 *       -> dma_map_single()
 *       -> slot[post_idx] = POSTED
 *
 * 【重要提示】
 * - 这是教学型实现，不用于生产环境
 * - 关注的是"异步模型"和"前后端边界"，不是高吞吐
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

#include "../include/netdev_stage08_compat.h"

/*
 * 【学习】模块参数
 * module_param_string / module_param
 * 允许通过 modprobe 或 insmod 时传入自定义值
 *
 * 例：insmod netdev_stage08.ko ifname=nds8 ring_size=256
 */

/*
 * 【学习】驱动名称和默认常量
 *
 * DRV_NAME：驱动名，用于 debugfs 目录名
 * STAGE08_DEFAULT_RING_SIZE = 128：默认 ring 深度
 * STAGE08_DEFAULT_NAPI_WEIGHT = 32：NAPI poll 一次最多处理 32 帧
 * STAGE08_DEFAULT_BACKEND_BATCH = 32：后端一次最多处理 32 帧
 */
#define DRV_NAME "netdev_stage08"
#define STAGE08_DEFAULT_RING_SIZE 128
#define STAGE08_DEFAULT_NAPI_WEIGHT 32
#define STAGE08_DEFAULT_RX_BUF_SIZE 2048
#define STAGE08_DEFAULT_BACKEND_BATCH 32
#define STAGE08_MAX_QUEUE_DUMP 16

/*
 * 【学习】slot 状态机
 *
 * TX slot 状态转换：
 *   FREE -> SUBMITTED（submit 时）-> DONE（backend 完成时）-> FREE（complete 时）
 *
 * RX slot 状态转换：
 *   FREE -> POSTED（refill 时）-> DONE（backend 产生 RX 时）-> FREE（consume 时）
 *
 * 【为什么需要 4 种状态】
 * - FREE：slot 可用，可被分配
 * - SUBMITTED/POSTED：slot 已被占用，等待处理
 * - DONE：处理完成，等待被清理
 */
enum stage08_slot_state {
    S08_SLOT_FREE = 0,       // slot 空闲，可被分配
    S08_SLOT_POSTED,          // RX：buffer 已posting，等待设备填充
    S08_SLOT_SUBMITTED,       // TX：skb 已提交，等待 backend 处理
    S08_SLOT_DONE,            // 处理完成，等待被 cleanup
};

/*
 * 【学习】descriptor vs buf_slot 分离设计
 *
 * stage08 的 queue 包含两个数组：
 * - desc[]：描述符数组，存储 DMA 地址和数据长度
 * - slots[]：slot 数组，存储 skb 指针和状态
 *
 * 【为什么分离】
 * - desc 是"设备侧"能看到的数据（DMA 地址）
 * - slots 是"驱动侧"的管理数据（skb 指针）
 * - 类似于 virtio-net 的 vring.desc 和 driver 侧的 buffer 管理
 */
struct stage08_desc {
    dma_addr_t dma_addr;  // 【学习】DMA 映射后的地址，设备用这个读写内存
    u32 data_len;          // 数据长度
    u16 state;            // 描述符状态
    u16 flags;            // 保留备用
};

struct stage08_buf_slot {
    struct sk_buff *skb;  // 【学习】对应的 skb，DMA 映射的目的地
    dma_addr_t dma_addr;  // DMA 地址（复制自 desc）
    u32 buf_len;          // buffer 总长度
    u32 data_len;         // 实际数据长度
    u16 state;            // slot 状态
    u16 id;               // slot 索引，用于调试
};

/*
 * 【学习】双 index 环
 *
 * stage08_queue 包含多个 index，用于跟踪 ring 状态：
 *
 * TX 相关 index：
 * - submit_idx：已提交，驱动侧可见
 * - notify_idx：已通知，backend 应该处理
 * - complete_idx：已完成，NAPI 可以回收
 *
 * RX 相关 index：
 * - post_idx：已 Posting，buffer 已分配，等待设备填充
 * - device_idx：设备侧可用，backend 正在填充
 * - consume_idx：已消费，NAPI 可以交给协议栈
 *
 * 【工作流程】
 * TX: submit_idx -> notify_idx -> complete_idx -> (清理) -> submit_idx
 * RX: post_idx -> device_idx -> consume_idx -> (refill) -> post_idx
 */
struct stage08_queue {
    struct stage08_desc *desc;   // 描述符数组
    struct stage08_buf_slot *slots; // slot 数组
    u16 size;                     // ring 大小

    /* TX indices */
    u16 submit_idx;    // 【学习】已提交，skb 已放入 slot
    u16 notify_idx;     // 【学习】已通知 backend，backend 应该处理
    u16 complete_idx;   // 【学习】TX 完成，NAPI 可以释放 skb

    /* RX indices */
    u16 post_idx;       // 【学习】已 posting，buffer 已分配
    u16 device_idx;     // 【学习】设备侧可用，backend 正在写
    u16 consume_idx;    // 【学习】已消费，NAPI 正在交给协议栈
};

/*
 * 【学习】timeline 结构
 *
 * 用于记录关键事件的时间戳（纳秒级），
 * 分析异步路径的延迟和时序关系。
 *
 * 各字段含义：
 * - last_submit_ns：协议栈调用 ndo_start_xmit 的时间
 * - last_doorbell_ns：mark_doorbell 被调用的时间
 * - last_backend_wakeup_ns：backend work 开始执行的时间
 * - last_backend_done_ns：backend work 完成处理的时间
 * - last_irq_ns：raise_irq 被调用的时间
 * - last_poll_ns：napi_poll 被调用的时间
 * - last_complete_ns：TX slot 被 cleanup 的时间
 * - last_consume_ns：RX skb 被 netif_receive_skb 的时间
 *
 * 关键 delta：
 * - delta_doorbell_to_backend_ns：doorbell 到 backend 执行的延迟（异步特征）
 * - delta_backend_to_irq_ns：backend 完成到 IRQ 的延迟
 * - delta_irq_to_poll_ns：IRQ 到 poll 的延迟
 */
struct stage08_timeline {
    u64 last_submit_ns;
    u64 last_doorbell_ns;
    u64 last_backend_wakeup_ns;
    u64 last_backend_done_ns;
    u64 last_irq_ns;
    u64 last_poll_ns;
    u64 last_complete_ns;
    u64 last_consume_ns;
};

/*
 * 【学习】stage08_priv — 驱动私有数据结构
 *
 * 重要字段：
 * - napi：NAPI 结构，用于批量收包
 * - txq/rxq：TX/RX 队列
 * - backend_wq/backend_work：后端工作队列和工作项
 * - doorbell_pending：是否有待处理的工作（避免重复调度）
 * - backend_running：backend 是否正在运行
 *
 * 统计字段（atomic64_t）：
 * - TX 相关：tx_submit_count, tx_complete_count, tx_packets, tx_dropped...
 * - RX 相关：rx_post_count, rx_consume_count, rx_packets, rx_dropped...
 * - NAPI 相关：irq_count, napi_poll_count, napi_complete_count...
 * - Backend 相关：backend_schedule_count, backend_run_count, backend_tx_processed...
 */
struct stage08_priv {
    struct net_device *ndev;
    struct napi_struct napi;         // 【学习】NAPI 结构，napi_poll 绑定的对象
    struct dentry *dbg_dir;            // debugfs 根目录

    spinlock_t state_lock;             // 【学习】保护所有共享状态的自旋锁

    struct stage08_queue txq;         // TX 队列
    struct stage08_queue rxq;         // RX 队列

    struct workqueue_struct *backend_wq;  // 【学习】后端工作队列（独立 CPU）
    struct work_struct backend_work;      // 【学习】后端工作项

    u32 rx_buf_size;                   // RX buffer 大小（默认 2048）
    u32 backend_delay_us;              // 【学习】模拟后端延迟（用于测试）
    u32 backend_batch;                 // 后端每次最大处理帧数

    bool irq_masked;                   // 【学习】IRQ 是否被屏蔽（NAPI 期间）
    bool doorbell_pending;            // 【学习】是否有待处理的工作
    bool backend_running;              // backend 是否正在运行

    u16 tx_inflight;                   // TX 飞行中的帧数
    u16 tx_done;                       // TX 已完成待清理的帧数
    u16 rx_posted;                     // RX 已 posting 的 buffer 数
    u16 rx_ready;                     // RX 已就绪可消费的帧数

    struct stage08_timeline timeline;  // timeline 统计

    /* Lifecycle stats */
    atomic64_t open_count;
    atomic64_t stop_count;

    /* Test-frame stats */
    atomic64_t test_tx_submit_count;
    atomic64_t test_backend_tx_processed;
    atomic64_t test_backend_rx_produced;
    atomic64_t test_rx_consume_count;
    atomic64_t last_test_proto;
    atomic64_t last_test_seq;

    /* TX stats */
    atomic64_t tx_submit_count;
    atomic64_t tx_complete_count;
    atomic64_t tx_packets;
    atomic64_t tx_bytes;
    atomic64_t tx_dropped;
    atomic64_t tx_busy;
    atomic64_t tx_linearize_count;
    atomic64_t tx_dma_map_ok;
    atomic64_t tx_dma_map_fail;
    atomic64_t tx_dma_unmap;

    /* RX stats */
    atomic64_t rx_post_count;
    atomic64_t rx_consume_count;
    atomic64_t rx_refill_count;
    atomic64_t rx_packets;
    atomic64_t rx_bytes;
    atomic64_t rx_dropped;
    atomic64_t rx_dma_map_ok;
    atomic64_t rx_dma_map_fail;
    atomic64_t rx_dma_unmap;
    atomic64_t rx_truncated;
    atomic64_t rx_no_posted;

    /* NAPI stats */
    atomic64_t irq_count;
    atomic64_t irq_mask_count;
    atomic64_t irq_unmask_count;
    atomic64_t napi_schedule_count;
    atomic64_t napi_poll_count;
    atomic64_t napi_complete_count;
    atomic64_t napi_budget_exhaust_count;
    atomic64_t napi_work_total;

    /* Backend stats */
    atomic64_t doorbell_count;
    atomic64_t backend_schedule_count;
    atomic64_t backend_run_count;
    atomic64_t backend_tx_processed;
    atomic64_t backend_rx_produced;
    atomic64_t backend_empty_runs;
    atomic64_t backend_requeue_count;

    /* Ring stats */
    atomic64_t ring_full_count;
    atomic64_t ring_empty_count;

    /* Last packet info */
    atomic64_t last_tx_len;
    atomic64_t last_tx_proto;
    atomic64_t last_rx_len;
    atomic64_t last_rx_proto;
};

static char ifname[IFNAMSIZ] = "nds8";
module_param_string(ifname, ifname, sizeof(ifname), 0644);
MODULE_PARM_DESC(ifname, "interface name for stage08 async backend transport");

static int ring_size = STAGE08_DEFAULT_RING_SIZE;
module_param(ring_size, int, 0644);
MODULE_PARM_DESC(ring_size, "TX/RX queue depth");

static int napi_weight = STAGE08_DEFAULT_NAPI_WEIGHT;
module_param(napi_weight, int, 0644);
MODULE_PARM_DESC(napi_weight, "NAPI poll weight");

static int rx_buf_size = STAGE08_DEFAULT_RX_BUF_SIZE;
module_param(rx_buf_size, int, 0644);
MODULE_PARM_DESC(rx_buf_size, "RX buffer size");

/*
 * 【学习】backend_delay_us 参数
 *
 * 这是一个测试用参数，用于模拟后端处理延迟。
 * 当设置为 > 0 时，backend_workfn() 会 sleep 指定微秒数。
 *
 * 用途：
 * - 测试异步模型在有延迟时是否仍能正常工作
 * - 观察 doorbell_pending 和 requeue 行为
 * - 模拟真实网卡的处理延迟
 */
static int backend_delay_us = 0;
module_param(backend_delay_us, int, 0644);
MODULE_PARM_DESC(backend_delay_us, "artificial backend delay in us");

static int backend_batch = STAGE08_DEFAULT_BACKEND_BATCH;
module_param(backend_batch, int, 0644);
MODULE_PARM_DESC(backend_batch, "max entries backend worker handles per run");

static struct net_device *stage08_dev;

/*
 * 【学习】ktime_get_ns() vs getrawmonotonic_ns()
 *
 * ktime_get_ns()：返回自系统启动以来的纳秒数，
 *                 会受到 suspend/resume 和时间调整的影响
 *
 * 本驱动使用 ktime_get_ns() 作为 timeline 基准，
 * 因为我们关心的是"同一系统内各事件的时间差"，
 * 而不是绝对时间。
 */

/*
 * 【学习】stage08_now_ns() — 获取当前时间戳
 *
 * 这是驱动内部的时间戳获取函数，
 * 用于填充 timeline 结构。
 */
#define STAGE08_TEST_ETHERTYPE 0x88B8
#define STAGE08_TEST_MAGIC "STAGE08"
#define STAGE08_TEST_MAGIC_LEN 7

struct stage08_test_info {
    bool matched;
    u16 proto;
    s32 seq;
};

/*
 * 【学习】stage08_now_ns — 获取当前纳秒时间戳
 *
 * 使用 ktime_get_ns() 获取原始硬件计数器，不受时间调整影响。
 * 这是 timeline 差分测量的基础，确保各阶段延迟精确可测量。
 */
static inline u64 stage08_now_ns(void)
{
    return ktime_get_ns();
}

/*
 * 【学习】stage08_parse_test_bytes — 解析测试帧的 payload
 *
 * 测试帧的 payload 格式：STAGE08 seq=N user=xxx
 * 这个函数负责：
 * 1. 检查 ETH_HLEN 偏移后是否有 STAGE08 magic
 * 2. 提取 seq 序号
 * 3. 验证 ethertype 是否为 0x88B8
 *
 * 返回值：info->matched=true 表示这是合法的测试帧
 */
static bool stage08_parse_test_bytes(const unsigned char *data, u32 len,
                     struct stage08_test_info *info)
{
    const unsigned char *payload;
    u32 payload_len;
    u32 i;
    unsigned int seq = 0;
    bool seen_digit = false;
    const char prefix[] = STAGE08_TEST_MAGIC " seq=";

    memset(info, 0, sizeof(*info));
    info->seq = -1;

    if (!data || len < ETH_HLEN + STAGE08_TEST_MAGIC_LEN)
        return false;

    info->proto = ((u16)data[12] << 8) | data[13];
    if (info->proto != STAGE08_TEST_ETHERTYPE)
        return false;

    payload = data + ETH_HLEN;
    payload_len = len - ETH_HLEN;
    if (payload_len < STAGE08_TEST_MAGIC_LEN ||
        memcmp(payload, STAGE08_TEST_MAGIC, STAGE08_TEST_MAGIC_LEN) != 0)
        return false;

    if (payload_len >= sizeof(prefix) - 1 &&
        memcmp(payload, prefix, sizeof(prefix) - 1) == 0) {
        for (i = sizeof(prefix) - 1; i < payload_len; ++i) {
            if (payload[i] < '0' || payload[i] > '9')
                break;
            seen_digit = true;
            seq = seq * 10 + (unsigned int)(payload[i] - '0');
        }
        if (seen_digit)
            info->seq = (s32)seq;
    }

    info->matched = true;
    return true;
}

/*
 * 【学习】stage08_account_test_submit — 记录测试帧的发送
 *
 * 只统计 magic 匹配的测试帧，用于 smoke test 的精确验证。
 * test_tx_submit_count 记录本次测试中 submit 的测试帧总数。
 */
static void stage08_account_test_submit(struct stage08_priv *priv,
                    const unsigned char *data, u32 len)
{
    struct stage08_test_info info;

    if (!stage08_parse_test_bytes(data, len, &info))
        return;

    atomic64_inc(&priv->test_tx_submit_count);
    atomic64_set(&priv->last_test_proto, info.proto);
    atomic64_set(&priv->last_test_seq, info.seq);
}

/*
 * 【学习】stage08_account_test_backend_tx — 记录 backend 处理的测试帧（TX 侧）
 *
 * backend 在批处理中消费 TX slot 时调用。
 * test_backend_tx_processed 证明 backend 确实处理了这些帧。
 */
static void stage08_account_test_backend_tx(struct stage08_priv *priv,
                        const unsigned char *data, u32 len)
{
    struct stage08_test_info info;

    if (!stage08_parse_test_bytes(data, len, &info))
        return;

    atomic64_inc(&priv->test_backend_tx_processed);
    atomic64_set(&priv->last_test_proto, info.proto);
    atomic64_set(&priv->last_test_seq, info.seq);
}

/*
 * 【学习】stage08_account_test_backend_rx — 记录 backend 产生的测试帧（RX 侧）
 *
 * backend 完成 memcpy（TX→RX）后调用。
 * test_backend_rx_produced 证明 backend 产生了对应的 RX 帧。
 */
static void stage08_account_test_backend_rx(struct stage08_priv *priv,
                        const unsigned char *data, u32 len)
{
    struct stage08_test_info info;

    if (!stage08_parse_test_bytes(data, len, &info))
        return;

    atomic64_inc(&priv->test_backend_rx_produced);
    atomic64_set(&priv->last_test_proto, info.proto);
    atomic64_set(&priv->last_test_seq, info.seq);
}

/*
 * 【学习】stage08_account_test_consume — 记录被协议栈消费的测试帧
 *
 * napi_poll -> consume_rx_one -> netif_receive_skb 链路末端调用。
 * test_rx_consume_count 证明 RX 帧最终被协议栈接收。
 * 这四个 account 函数形成完整链路：submit → backend_tx → backend_rx → consume
 */
static void stage08_account_test_consume(struct stage08_priv *priv,
                     const unsigned char *data, u32 len)
{
    struct stage08_test_info info;

    if (!stage08_parse_test_bytes(data, len, &info))
        return;

    atomic64_inc(&priv->test_rx_consume_count);
    atomic64_set(&priv->last_test_proto, info.proto);
    atomic64_set(&priv->last_test_seq, info.seq);
}

/*
 * 【学习】环形 buffer 的 index 递增
 *
 * (idx + 1) % size 实现环形递增，
 * 避免使用 if-else 或条件分支。
 *
 * 注意：size 必须是 2 的幂时，才可以用位运算优化
 *       (idx + 1) & (size - 1)
 *       但当前使用取模，更通用
 */
static inline u16 stage08_next_idx(u16 idx, u16 size)
{
    return (u16)((idx + 1) % size);
}

/*
 * 【学习】状态名称转换（用于 debugfs 输出）
 */
static const char *stage08_state_name(u16 state)
{
    switch (state) {
    case S08_SLOT_FREE: return "FREE";
    case S08_SLOT_POSTED: return "POSTED";
    case S08_SLOT_SUBMITTED: return "SUBMITTED";
    case S08_SLOT_DONE: return "DONE";
    default: return "?";
    }
}

/*
 * 【学习】DMA mask 设置
 *
 * 网络设备通常需要 64-bit DMA能力，
 * 因为它们需要 DMA 到高端内存。
 *
 * 设置顺序：
 * 1. 先设置 coherent_dma_mask（用于一致性 DMA）
 * 2. 再设置 dma_mask（用于流式 DMA）
 * 3. 调用 dma_set_mask_and_coherent()
 *
 * 失败时回退到 32-bit DMA
 */
static int stage08_prepare_dma_caps(struct net_device *ndev)
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
 * 【学习】stage08_mark_doorbell — 触发后端
 *
 * 这是前后端分离的关键函数：
 * 1. 设置 doorbell_pending = true（标记有待处理工作）
 * 2. 记录 doorbell 时间戳
 * 3. 增加 backend_schedule_count（调度计数）
 * 4. queue_work() 把 backend_work 加入工作队列
 *
 * 【为什么需要 doorbell_pending】
 * - 避免重复调度：如果 backend 正在运行，不应该再调度
 * - 调度后，如果还有未完成工作，需要再次调度
 *
 * 【关键点】
 * - doorbell 是"同步调用，异步执行"
 * - submit 函数立即返回，不等待 backend 完成
 * - 这就是"前后端分离"的核心
 */
static void stage08_mark_doorbell(struct stage08_priv *priv)
{
    unsigned long flags;

    spin_lock_irqsave(&priv->state_lock, flags);
    priv->doorbell_pending = true;
    priv->timeline.last_doorbell_ns = stage08_now_ns();
    spin_unlock_irqrestore(&priv->state_lock, flags);

    atomic64_inc(&priv->doorbell_count);
    atomic64_inc(&priv->backend_schedule_count);
    queue_work(priv->backend_wq, &priv->backend_work);
}

/*
 * 【学习】stage08_raise_irq — 触发软中断
 *
 * 前后端分离的关键：
 * - backend 处理完成后，不直接调用 NAPI
 * - 而是 raise IRQ，触发软中断处理
 *
 * 为什么这么做：
 * 1. 模拟真实硬件行为（设备通过 IRQ 通知驱动）
 * 2. 利用 Linux 的中断处理机制
 * 3. NAPI 只能在软中断上下文调用
 *
 * 【irq_masked 机制】
 * - 当 NAPI 正在运行时，设置 irq_masked = true
 * - 此时 raise_irq() 不会再次触发 NAPI
 * - 避免重复调度
 */
static void stage08_raise_irq(struct stage08_priv *priv)
{
    bool do_schedule = false;
    unsigned long flags;

    spin_lock_irqsave(&priv->state_lock, flags);
    if (!priv->irq_masked) {
        priv->irq_masked = true;
        priv->timeline.last_irq_ns = stage08_now_ns();
        do_schedule = true;
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);

    if (!do_schedule)
        return;

    atomic64_inc(&priv->irq_count);
    atomic64_inc(&priv->irq_mask_count);
    atomic64_inc(&priv->napi_schedule_count);
    napi_schedule(&priv->napi);
}

/*
 * 【学习】stage08_post_rx_slot — 分配并 posting 一个 RX buffer
 *
 * 这是 RX replenishment 的核心函数：
 * 1. 分配一个 skb（netdev_alloc_skb_ip_align）
 * 2. DMA 映射 skb->data
 * 3. 设置 slot 状态为 POSTED
 * 4. 更新 post_idx
 *
 * 【为什么需要 DMA 映射】
 * - 网络设备需要 DMA 到 skb->data 缓冲区
 * - 驱动需要先映射，获得 DMA 地址
 * - 设备通过 DMA 填充数据到 buffer
 *
 * 【netdev_alloc_skb_ip_align】
 * - 分配 SKB 并对齐到 16 字节边界
 * - ip_align 通常是 2 字节，用于 IP 头对齐
 */
static int stage08_post_rx_slot(struct stage08_priv *priv, u16 idx)
{
    struct stage08_buf_slot *slot = &priv->rxq.slots[idx];
    struct stage08_desc *desc = &priv->rxq.desc[idx];
    struct sk_buff *skb;
    dma_addr_t dma_addr;
    unsigned long flags;

    /* 【学习】分配 SKB，2 字节对齐 */
    skb = netdev_alloc_skb_ip_align(priv->ndev, priv->rx_buf_size);
    if (!skb) {
        atomic64_inc(&priv->rx_dma_map_fail);
        return -ENOMEM;
    }

    /* 【学习】DMA FROM_DEVICE：设备 -> 内存（接收数据）*/
    dma_addr = dma_map_single(&priv->ndev->dev, skb->data, priv->rx_buf_size, DMA_FROM_DEVICE);
    if (dma_mapping_error(&priv->ndev->dev, dma_addr)) {
        dev_kfree_skb_any(skb);
        atomic64_inc(&priv->rx_dma_map_fail);
        return -EIO;
    }

    /* 【学习】确保 CPU 看到 DMA 地址对应的内存 */
    dma_sync_single_for_device(&priv->ndev->dev, dma_addr, priv->rx_buf_size, DMA_FROM_DEVICE);
    atomic64_inc(&priv->rx_dma_map_ok);

    spin_lock_irqsave(&priv->state_lock, flags);
    slot->skb = skb;
    slot->dma_addr = dma_addr;
    slot->buf_len = priv->rx_buf_size;
    slot->data_len = 0;
    slot->state = S08_SLOT_POSTED;
    desc->dma_addr = dma_addr;
    desc->data_len = 0;
    desc->state = S08_SLOT_POSTED;
    priv->rx_posted++;
    priv->rxq.post_idx = stage08_next_idx(idx, priv->rxq.size);
    spin_unlock_irqrestore(&priv->state_lock, flags);

    atomic64_inc(&priv->rx_post_count);
    atomic64_inc(&priv->rx_refill_count);
    return 0;
}

/*
 * 【学习】stage08_refill_one — 补充一个 RX buffer
 *
 * 这是一个"尝试补充一个 RX buffer"的函数：
 * 1. 检查 post_idx 指向的 slot 是否为 FREE
 * 2. 如果是，调用 post_rx_slot 分配 buffer
 * 3. 如果不是，说明 ring 满了（ring_full_count++）
 *
 * 【什么时候被调用】
 * - consume_rx_one() 消费一个 RX skb 后
 * - stage08_alloc_queues() 初始化时预填充 ring
 */
static int stage08_refill_one(struct stage08_priv *priv)
{
    u16 idx;
    unsigned long flags;

    spin_lock_irqsave(&priv->state_lock, flags);
    idx = priv->rxq.post_idx;
    if (priv->rxq.slots[idx].state != S08_SLOT_FREE) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        atomic64_inc(&priv->ring_full_count);
        return -EBUSY;
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);

    return stage08_post_rx_slot(priv, idx);
}

/*
 * 【学习】stage08_reset_queue_state — 重置队列状态
 *
 * 在 ndo_open() 或重新初始化时被调用：
 * 1. 重置所有 index 到 0
 * 2. 重置 inflight/done/posted/ready 计数
 * 3. 重置 irq_masked, doorbell_pending, backend_running
 * 4. 清零 timeline
 */
static void stage08_reset_queue_state(struct stage08_priv *priv)
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
    priv->doorbell_pending = false;
    priv->backend_running = false;
    memset(&priv->timeline, 0, sizeof(priv->timeline));
    atomic64_set(&priv->last_test_proto, 0);
    atomic64_set(&priv->last_test_seq, -1);
    spin_unlock_irqrestore(&priv->state_lock, flags);
}

/*
 * 【学习】stage08_backend_workfn — 后端工作函数
 *
 * 这是前后端分离的核心：
 * 1. 在独立 workqueue 上异步执行
 * 2. 处理 TX 帧：从 TX ring 取出，环回到 RX ring
 * 3. 产生 RX 帧：模拟设备接收
 * 4. 触发 IRQ：通知前端有数据到达
 *
 * 【关键概念】
 * - backend 是"模拟设备"，不是真实硬件
 * - 它接收 TX skb，产生 RX skb（模拟环回）
 * - 这是教学模型，真实网卡不会这样做
 *
 * 【工作流程】
 * while (有待处理 TX && 有可用 RX buffer && 未达 batch 限制) {
 *     1. 取 TX slot[notify_idx]
 *     2. 取 RX slot[device_idx]
 *     3. 复制 TX data -> RX skb->data（memcpy）
 *     4. 设置 RX slot 为 DONE
 *     5. 设置 TX slot 为 DONE
 *     6. 更新 indices
 * }
 * if (处理了任何帧) raise_irq()
 * if (还有未处理 TX && 有可用 RX) {
 *     doorbell_pending = true
 *     再调度自己
 * }
 */
static void stage08_backend_workfn(struct work_struct *work)
{
    struct stage08_priv *priv = container_of(work, struct stage08_priv, backend_work);
    bool produced = false;
    unsigned long flags;
    int loops = 0;

    atomic64_inc(&priv->backend_run_count);

    /* 记录 backend 开始时间，设置 running 标记 */
    spin_lock_irqsave(&priv->state_lock, flags);
    priv->backend_running = true;
    priv->timeline.last_backend_wakeup_ns = stage08_now_ns();
    priv->doorbell_pending = false;
    spin_unlock_irqrestore(&priv->state_lock, flags);

    /* 【学习】可选延迟：模拟设备处理时间 */
    if (priv->backend_delay_us > 0)
        udelay(priv->backend_delay_us);

    /*
     * 【学习】批处理循环
     *
     * 处理条件：
     * - TX notify_idx != submit_idx：还有待处理的 TX
     * - rx_posted > 0：有可用的 RX buffer
     * - loops < backend_batch：未达到批次限制
     */
    spin_lock_irqsave(&priv->state_lock, flags);
    while (priv->txq.notify_idx != priv->txq.submit_idx &&
           priv->rx_posted > 0 &&
           loops < priv->backend_batch) {
        struct stage08_desc *txd = &priv->txq.desc[priv->txq.notify_idx];
        struct stage08_buf_slot *txs = &priv->txq.slots[priv->txq.notify_idx];
        struct stage08_desc *rxd = &priv->rxq.desc[priv->rxq.device_idx];
        struct stage08_buf_slot *rxs = &priv->rxq.slots[priv->rxq.device_idx];
        u32 copy_len;

        /*
         * 【学习】安全检查：确认 slot 状态正确
         * - TX slot 必须是 SUBMITTED 状态
         * - TX slot 必须有有效的 skb
         * - RX slot 必须是 POSTED 状态
         * - RX slot 必须有有效的 skb
         */
        if (txd->state != S08_SLOT_SUBMITTED || txs->state != S08_SLOT_SUBMITTED || !txs->skb)
            break;
        if (rxd->state != S08_SLOT_POSTED || rxs->state != S08_SLOT_POSTED || !rxs->skb)
            break;

        /*
         * 【学习】DMA 同步 + 数据复制
         *
         * 1. dma_sync_single_for_device：
         *    确保设备可以看到最新的 memory 内容
         *    （如果之前 CPU 写过，需要 sync 到设备）
         *
         * 2. memcpy：
         *    将 TX skb 的数据复制到 RX skb 的 buffer
         *    这是"软环回"的核心操作
         *
         * 3. dma_sync_single_for_cpu：
         *    复制完成后，确保 CPU 可以读取
         */
        copy_len = min_t(u32, txd->data_len, rxs->buf_len);
        if (copy_len < txd->data_len)
            atomic64_inc(&priv->rx_truncated);

        dma_sync_single_for_device(&priv->ndev->dev, rxs->dma_addr, rxs->buf_len, DMA_FROM_DEVICE);
        memcpy(rxs->skb->data, txs->skb->data, copy_len);
        dma_sync_single_for_cpu(&priv->ndev->dev, rxs->dma_addr, copy_len, DMA_FROM_DEVICE);
        stage08_account_test_backend_tx(priv, txs->skb->data, txd->data_len);
        stage08_account_test_backend_rx(priv, rxs->skb->data, copy_len);

        /*
         * 【学习】标记 RX slot 为 DONE，更新 device_idx
         *
         * 注意：这里只是"设备侧可用"，还不是"消费就绪"
         * consume_rx_one() 会在 poll 中处理
         */
        rxs->data_len = copy_len;
        rxs->state = S08_SLOT_DONE;
        rxd->data_len = copy_len;
        rxd->state = S08_SLOT_DONE;
        priv->rx_ready++;
        if (priv->rx_posted > 0)
            priv->rx_posted--;
        priv->rxq.device_idx = stage08_next_idx(priv->rxq.device_idx, priv->rxq.size);
        atomic64_inc(&priv->backend_rx_produced);

        /*
         * 【学习】标记 TX slot 为 DONE，更新 notify_idx
         *
         * 注意：这里只是"backend 完成"，还不是"协议栈确认"
         * stage08_complete_tx_one() 会在 poll 中清理
         */
        txs->state = S08_SLOT_DONE;
        txd->state = S08_SLOT_DONE;
        priv->tx_done++;
        priv->txq.notify_idx = stage08_next_idx(priv->txq.notify_idx, priv->txq.size);
        atomic64_inc(&priv->backend_tx_processed);
        produced = true;
        loops++;
    }

    /* 【学习】错误计数 */
    if (priv->txq.notify_idx != priv->txq.submit_idx && priv->rx_posted == 0)
        atomic64_inc(&priv->rx_no_posted);
    if (loops == 0)
        atomic64_inc(&priv->backend_empty_runs);

    priv->timeline.last_backend_done_ns = stage08_now_ns();
    priv->backend_running = false;

    /*
     * 【学习】Requeue 机制
     *
     * 如果：
     * - 还有未处理的 TX（notify_idx != submit_idx）
     * - 有可用的 RX buffer（rx_posted > 0）
     *
     * 则设置 doorbell_pending = true，触发再次调度
     *
     * 【关键问题】
     * 为什么不用 while 循环一次性处理完？
     * 因为：
     * 1. budget 限制：避免一次处理太多，影响调度公平性
     * 2. 模拟真实设备：真实硬件的处理能力有限
     * 3. 配合 NAPI：让出 CPU 给协议栈处理
     */
    if (priv->txq.notify_idx != priv->txq.submit_idx && priv->rx_posted > 0) {
        priv->doorbell_pending = true;
        atomic64_inc(&priv->backend_requeue_count);
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);

    /*
     * 【学习】触发 IRQ
     *
     * 如果处理了任何帧（produced == true），
     * 调用 raise_irq() 触发软中断，
     * 通知前端有 RX 数据就绪。
     *
     * 注意：raise_irq() 内部会检查 irq_masked，
     * 如果 NAPI 正在运行，不会重复触发。
     */
    if (produced)
        stage08_raise_irq(priv);

    /*
     * 【学习】Requeue 触发
     *
     * 调度结束后，如果还有待处理工作，
     * 再次把 backend_work 加入队列。
     *
     * 这是一种"工作接力"模式：
     * backend_workfn 结束时，如果还有活，再调度自己。
     */
    spin_lock_irqsave(&priv->state_lock, flags);
    if (priv->doorbell_pending && !priv->backend_running) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        queue_work(priv->backend_wq, &priv->backend_work);
        return;
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);
}

/*
 * 【学习】stage08_complete_tx_one — 清理一个完成的 TX slot
 *
 * 这是 TX 路径的最后一步：
 * 1. 检查 tx_done 是否有完成的 slot
 * 2. 取出 complete_idx 指向的 slot
 * 3. DMA unmap 释放 DMA 映射
 * 4. 释放 skb
 * 5. 重置 slot 为 FREE
 * 6. 更新 complete_idx 和 tx_inflight
 * 7. 如果队列被 stop 了，且现在有空间，wake up 队列
 *
 * 【调用时机】
 * - napi_poll() 中被循环调用
 * - 直到没有更多完成的 TX slot
 */
static int stage08_complete_tx_one(struct stage08_priv *priv)
{
    struct stage08_buf_slot saved = { 0 };
    u16 idx;
    unsigned long flags;

    spin_lock_irqsave(&priv->state_lock, flags);
    if (!priv->tx_done) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        return 0;
    }

    idx = priv->txq.complete_idx;
    if (priv->txq.desc[idx].state != S08_SLOT_DONE ||
        priv->txq.slots[idx].state != S08_SLOT_DONE ||
        !priv->txq.slots[idx].skb) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        return 0;
    }

    /*
     * 【学习】保存 slot 数据并重置
     *
     * 注意：必须先保存，因为 slot 会被 memset 清零
     * saved.skb 和 saved.dma_addr 在 unlock 后还需要使用
     */
    saved = priv->txq.slots[idx];
    memset(&priv->txq.slots[idx], 0, sizeof(priv->txq.slots[idx]));
    priv->txq.slots[idx].id = idx;
    priv->txq.slots[idx].state = S08_SLOT_FREE;
    memset(&priv->txq.desc[idx], 0, sizeof(priv->txq.desc[idx]));
    priv->txq.desc[idx].state = S08_SLOT_FREE;
    priv->txq.complete_idx = stage08_next_idx(priv->txq.complete_idx, priv->txq.size);
    priv->tx_done--;
    if (priv->tx_inflight > 0)
        priv->tx_inflight--;
    priv->timeline.last_complete_ns = stage08_now_ns();
    spin_unlock_irqrestore(&priv->state_lock, flags);

    /*
     * 【学习】DMA unmap 和 skb 释放
     *
     * 注意：
     * - 必须在 spin_unlock 之后调用（因为可能 sleep）
     * - tx_dma_unmap 计数用于统计
     */
    dma_unmap_single(&priv->ndev->dev, saved.dma_addr, saved.data_len, DMA_TO_DEVICE);
    atomic64_inc(&priv->tx_dma_unmap);
    atomic64_inc(&priv->tx_complete_count);

    dev_consume_skb_any(saved.skb);

    /* 【学习】队列 wake up */
    if (netif_queue_stopped(priv->ndev))
        netif_wake_queue(priv->ndev);
    return 1;
}

/*
 * 【学习】stage08_consume_rx_one — 消费一个 RX 帧
 *
 * 这是 RX 路径的核心：
 * 1. 检查 rx_ready 是否有就绪的 RX slot
 * 2. 取出 consume_idx 指向的 slot
 * 3. DMA unmap 释放 DMA 映射
 * 4. 设置 skb 长度（skb_put）
 * 5. 调用 eth_type_trans() 确定协议
 * 6. 调用 netif_receive_skb() 交给协议栈
 * 7. refill 一个新 buffer
 *
 * 【调用时机】
 * - napi_poll() 中被循环调用
 * - 直到 budget 用完或没有更多 RX slot
 *
 * 【eth_type_trans】
 * - 确定数据包的协议类型（IPv4/IPv6/ARP...）
 * - 设置 skb->pkt_type 和 skb->protocol
 */
static int stage08_consume_rx_one(struct stage08_priv *priv)
{
    struct stage08_buf_slot saved = { 0 };
    u16 idx;
    __be16 proto;
    unsigned long flags;

    spin_lock_irqsave(&priv->state_lock, flags);
    if (!priv->rx_ready) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        return 0;
    }

    idx = priv->rxq.consume_idx;
    if (priv->rxq.desc[idx].state != S08_SLOT_DONE ||
        priv->rxq.slots[idx].state != S08_SLOT_DONE ||
        !priv->rxq.slots[idx].skb) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        return 0;
    }

    saved = priv->rxq.slots[idx];
    memset(&priv->rxq.slots[idx], 0, sizeof(priv->rxq.slots[idx]));
    priv->rxq.slots[idx].id = idx;
    priv->rxq.slots[idx].state = S08_SLOT_FREE;
    memset(&priv->rxq.desc[idx], 0, sizeof(priv->rxq.desc[idx]));
    priv->rxq.desc[idx].state = S08_SLOT_FREE;
    priv->rxq.consume_idx = stage08_next_idx(priv->rxq.consume_idx, priv->rxq.size);
    if (priv->rx_ready > 0)
        priv->rx_ready--;
    priv->timeline.last_consume_ns = stage08_now_ns();
    spin_unlock_irqrestore(&priv->state_lock, flags);

    /*
     * 【学习】DMA FROM_DEVICE unmap
     *
     * 注意：
     * - 使用 DMA_FROM_DEVICE（设备 -> 内存）
     * - 长度是 buf_len，不是 data_len
     */
    dma_unmap_single(&priv->ndev->dev, saved.dma_addr, saved.buf_len, DMA_FROM_DEVICE);
    atomic64_inc(&priv->rx_dma_unmap);

    /* 【学习】skb_put + eth_type_trans + netif_receive_skb */
    skb_put(saved.skb, saved.data_len);
    stage08_account_test_consume(priv, saved.skb->data, saved.data_len);
    proto = eth_type_trans(saved.skb, priv->ndev);
    netif_receive_skb(saved.skb);

    atomic64_inc(&priv->rx_consume_count);
    atomic64_inc(&priv->rx_packets);
    atomic64_add(saved.data_len, &priv->rx_bytes);
    atomic64_set(&priv->last_rx_len, saved.data_len);
    atomic64_set(&priv->last_rx_proto, ntohs(proto));

    /*
     * 【学习】RX replenishment
     *
     * 消费完一个 RX slot 后，
     * 必须立即补充一个 buffer，
     * 否则 RX ring 会很快耗尽。
     *
     * 注意：
     * - refill_one() 可能失败（比如 memory pressure）
     * - 失败时增加 rx_dropped 计数
     * - 但仍然返回 1（表示消费成功）
     */
    if (stage08_refill_one(priv))
        atomic64_inc(&priv->rx_dropped);
    return 1;
}

/*
 * 【学习】stage08_poll — NAPI poll 函数
 *
 * 这是前后端合并的关键点：
 * 1. 先完成 TX slot 的清理（complete_tx_one）
 * 2. 再消费 RX slot（consume_rx_one）
 * 3. 根据 budget 决定是否完成 poll
 *
 * 【poll 函数的职责】
 * - 返回值 work_done：本次 poll 处理了多少帧
 * - 如果 work_done < budget：说明没有更多工作了，可以 complete
 * - 如果 work_done == budget：可能还有工作，下次 poll 再处理
 *
 * 【NAPI  completion】
 * - napi_complete_done() 标记本次 poll 完成
 * - 如果还有 rx_ready，重新调度 NAPI
 * - 如果 doorbell_pending，重新调度 backend
 *
 * 【中断抑制】
 * - 关键机制：irq_masked 控制是否响应 IRQ
 * - NAPI 运行期间：irq_masked = true，IRQ 不会触发 NAPI
 * - poll 结束且无更多工作时：irq_masked = false，允许新 IRQ
 */
static int stage08_poll(struct napi_struct *napi, int budget)
{
    struct stage08_priv *priv = container_of(napi, struct stage08_priv, napi);
    int work_done = 0;
    bool more_rx = false;
    unsigned long flags;

    atomic64_inc(&priv->napi_poll_count);
    spin_lock_irqsave(&priv->state_lock, flags);
    priv->timeline.last_poll_ns = stage08_now_ns();
    spin_unlock_irqrestore(&priv->state_lock, flags);

    /*
     * 【学习】TX completion 循环
     *
     * 注意：这里用 while，会尽可能清理所有完成的 TX slot
     * 不受 budget 限制，因为 TX completion 不占用协议栈 CPU 时间
     */
    while (stage08_complete_tx_one(priv))
        ;

    /*
     * 【学习】RX consume 循环
     *
     * 这里受 budget 限制：
     * - 每处理一帧 budget--
     * - work_done++ 记录处理数量
     * - budget 用完时退出
     *
     * 【为什么需要 budget】
     * - 避免 poll 占太多 CPU 时间
     * - 让其他软中断有机会运行
     * - 公平调度
     */
    while (work_done < budget && stage08_consume_rx_one(priv))
        work_done++;

    atomic64_add(work_done, &priv->napi_work_total);
    if (work_done == budget)
        atomic64_inc(&priv->napi_budget_exhaust_count);

    spin_lock_irqsave(&priv->state_lock, flags);
    more_rx = priv->rx_ready > 0;
    if (!more_rx) {
        napi_complete_done(napi, work_done);
        priv->irq_masked = false;
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);

    if (!more_rx) {
        atomic64_inc(&priv->napi_complete_count);
        atomic64_inc(&priv->irq_unmask_count);
    }

    /*
     * 【学习】Requeue 检查
     *
     * poll 结束后：
     * 1. 如果 doorbell_pending，重新调度 backend
     * 2. backend 会处理剩余的 TX slot
     */
    if (!more_rx && priv->doorbell_pending)
        queue_work(priv->backend_wq, &priv->backend_work);

    return work_done;
}

/*
 * 【学习】stage08_xmit — ndo_start_xmit 实现
 *
 * TX 路径的入口函数：
 * 1. 处理非线性 skb（如果需要 linearize）
 * 2. DMA 映射 skb->data
 * 3. 检查 ring 是否有空间
 * 4. 放入 TX slot，设置状态为 SUBMITTED
 * 5. 更新 submit_idx 和 inflight
 * 6. 调用 doorbell 触发 backend
 *
 * 【返回值】
 * - NETDEV_TX_OK：发送成功（帧已入 ring）
 * - NETDEV_TX_BUSY：发送失败（ring 满），需要重试
 * - 注意：即使返回 BUSY，skb 也已经被 free 了（避免泄漏）
 *
 * 【NETDEV_TX_OK vs NETDEV_TX_BUSY】
 * - OK：帧已进入驱动的 TX ring，等待 backend 处理
 * - BUSY：驱动的 TX ring 已满，协议栈应该稍后重试
 * - 关键：调用者会在 BUSY 时调用 netif_stop_queue()
 * - 驱动在 complete_tx_one() 中调用 netif_wake_queue()
 */
static netdev_tx_t stage08_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    struct stage08_priv *priv = netdev_priv(ndev);
    struct stage08_desc *txd;
    struct stage08_buf_slot *txs;
    dma_addr_t tx_dma;
    u16 idx;
    unsigned long flags;

    /* 【学习】非线性 skb 处理 */
    if (unlikely(skb_is_nonlinear(skb))) {
        if (skb_linearize(skb)) {
            atomic64_inc(&priv->tx_dropped);
            dev_kfree_skb_any(skb);
            return NETDEV_TX_OK;
        }
        atomic64_inc(&priv->tx_linearize_count);
    }

    /* 【学习】DMA TO_DEVICE 映射 */
    tx_dma = dma_map_single(&ndev->dev, skb->data, skb->len, DMA_TO_DEVICE);
    if (dma_mapping_error(&ndev->dev, tx_dma)) {
        atomic64_inc(&priv->tx_dma_map_fail);
        atomic64_inc(&priv->tx_dropped);
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }
    atomic64_inc(&priv->tx_dma_map_ok);

    spin_lock_irqsave(&priv->state_lock, flags);

    /* 【学习】Ring 满检查 */
    if (priv->tx_inflight >= priv->txq.size) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        atomic64_inc(&priv->tx_busy);
        atomic64_inc(&priv->ring_full_count);
        netif_stop_queue(ndev);
        dma_unmap_single(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);
        return NETDEV_TX_BUSY;
    }

    idx = priv->txq.submit_idx;
    if (priv->txq.slots[idx].state != S08_SLOT_FREE) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        atomic64_inc(&priv->tx_busy);
        atomic64_inc(&priv->ring_full_count);
        netif_stop_queue(ndev);
        dma_unmap_single(&ndev->dev, tx_dma, skb->len, DMA_TO_DEVICE);
        return NETDEV_TX_BUSY;
    }

    /* 【学习】填充 TX slot */
    txs = &priv->txq.slots[idx];
    txd = &priv->txq.desc[idx];
    txs->skb = skb;
    txs->dma_addr = tx_dma;
    txs->buf_len = skb->len;
    txs->data_len = skb->len;
    txs->state = S08_SLOT_SUBMITTED;
    txd->dma_addr = tx_dma;
    txd->data_len = skb->len;
    txd->state = S08_SLOT_SUBMITTED;

    priv->txq.submit_idx = stage08_next_idx(priv->txq.submit_idx, priv->txq.size);
    priv->tx_inflight++;
    priv->timeline.last_submit_ns = stage08_now_ns();
    spin_unlock_irqrestore(&priv->state_lock, flags);

    atomic64_inc(&priv->tx_submit_count);
    atomic64_inc(&priv->tx_packets);
    atomic64_add(skb->len, &priv->tx_bytes);
    atomic64_set(&priv->last_tx_len, skb->len);
    atomic64_set(&priv->last_tx_proto, ntohs(skb->protocol));

    /* 【学习】Doorbell */
    stage08_mark_doorbell(priv);
    return NETDEV_TX_OK;
}

/*
 * 【学习】stage08_open / stage08_stop
 *
 * ndo_open：启用网卡
 * - 启用 NAPI
 * - 启动 TX 队列
 * - 增加 open_count
 *
 * ndo_stop：禁用网卡
 * - 停止 TX 队列
 * - 禁用 NAPI
 * - 取消 backend work
 * - 增加 stop_count
 */
static int stage08_open(struct net_device *ndev)
{
    struct stage08_priv *priv = netdev_priv(ndev);

    napi_enable(&priv->napi);
    netif_start_queue(ndev);
    atomic64_inc(&priv->open_count);
    return 0;
}

static int stage08_stop(struct net_device *ndev)
{
    struct stage08_priv *priv = netdev_priv(ndev);

    netif_stop_queue(ndev);
    napi_disable(&priv->napi);
    cancel_work_sync(&priv->backend_work);
    atomic64_inc(&priv->stop_count);
    return 0;
}

/*
 * 【学习】stage08_get_stats64
 *
 * ndo_get_stats64 实现
 * - 被 ip -s link show 调用
 * - 返回 driver 维护的统计值
 *
 * 注意：
 * - 这里返回的是 atomic64_read() 的值
 * - 不是 net_device 内部的 counters
 */
static void stage08_get_stats64(struct net_device *ndev, struct rtnl_link_stats64 *stats)
{
    struct stage08_priv *priv = netdev_priv(ndev);

    stats->tx_packets = atomic64_read(&priv->tx_packets);
    stats->tx_bytes = atomic64_read(&priv->tx_bytes);
    stats->tx_dropped = atomic64_read(&priv->tx_dropped);
    stats->rx_packets = atomic64_read(&priv->rx_packets);
    stats->rx_bytes = atomic64_read(&priv->rx_bytes);
    stats->rx_dropped = atomic64_read(&priv->rx_dropped);
}

/*
 * 【学习】net_device_ops
 *
 * 函数指针表，定义网卡的回调函数
 * - ndo_open：启用
 * - ndo_stop：停用
 * - ndo_start_xmit：发送
 * - ndo_get_stats64：获取统计
 */
static const struct net_device_ops stage08_netdev_ops = {
    .ndo_open = stage08_open,
    .ndo_stop = stage08_stop,
    .ndo_start_xmit = stage08_xmit,
    .ndo_get_stats64 = stage08_get_stats64,
};

/*
 * 【学习】debugfs stats show
 *
 * 通过 /sys/kernel/debug/netdev_stage08/stats 查看详细统计
 *
 * P64 宏：
 * - 展开为 seq_printf(m, "name=%lld\n", atomic64_read(&priv->name))
 * - 简化了重复的打印代码
 */
static int stage08_stats_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage08_priv *priv = netdev_priv(ndev);

#define P64(name) seq_printf(m, #name "=%lld\n", atomic64_read(&priv->name))
    P64(open_count);
    P64(stop_count);
    P64(tx_submit_count);
    P64(tx_complete_count);
    P64(tx_packets);
    P64(tx_bytes);
    P64(tx_dropped);
    P64(tx_busy);
    P64(tx_linearize_count);
    P64(tx_dma_map_ok);
    P64(tx_dma_map_fail);
    P64(tx_dma_unmap);

    P64(rx_post_count);
    P64(rx_consume_count);
    P64(rx_refill_count);
    P64(rx_packets);
    P64(rx_bytes);
    P64(rx_dropped);
    P64(rx_dma_map_ok);
    P64(rx_dma_map_fail);
    P64(rx_dma_unmap);
    P64(rx_truncated);
    P64(rx_no_posted);

    P64(irq_count);
    P64(irq_mask_count);
    P64(irq_unmask_count);
    P64(napi_schedule_count);
    P64(napi_poll_count);
    P64(napi_complete_count);
    P64(napi_budget_exhaust_count);
    P64(napi_work_total);

    P64(doorbell_count);
    P64(backend_schedule_count);
    P64(backend_run_count);
    P64(backend_tx_processed);
    P64(backend_rx_produced);
    P64(backend_empty_runs);
    P64(backend_requeue_count);
    P64(ring_full_count);
    P64(ring_empty_count);
#undef P64

    seq_printf(m, "tx_inflight=%u\n", priv->tx_inflight);
    seq_printf(m, "tx_done=%u\n", priv->tx_done);
    seq_printf(m, "rx_posted=%u\n", priv->rx_posted);
    seq_printf(m, "rx_ready=%u\n", priv->rx_ready);
    seq_printf(m, "doorbell_pending=%u\n", priv->doorbell_pending ? 1 : 0);
    seq_printf(m, "backend_running=%u\n", priv->backend_running ? 1 : 0);
    seq_printf(m, "backend_delay_us=%u\n", priv->backend_delay_us);
    seq_printf(m, "backend_batch=%u\n", priv->backend_batch);
    seq_printf(m, "last_tx_len=%lld\n", atomic64_read(&priv->last_tx_len));
    seq_printf(m, "last_tx_proto=0x%llx\n", atomic64_read(&priv->last_tx_proto));
    seq_printf(m, "last_rx_len=%lld\n", atomic64_read(&priv->last_rx_len));
    seq_printf(m, "last_rx_proto=0x%llx\n", atomic64_read(&priv->last_rx_proto));
    return 0;
}

static int stage08_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage08_stats_show, inode->i_private);
}

static const struct file_operations stage08_stats_fops = {
    .owner = THIS_MODULE,
    .open = stage08_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int stage08_test_stats_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage08_priv *priv = netdev_priv(ndev);

#define TP64(name) seq_printf(m, #name "=%lld\n", atomic64_read(&priv->name))
    TP64(test_tx_submit_count);
    TP64(test_backend_tx_processed);
    TP64(test_backend_rx_produced);
    TP64(test_rx_consume_count);
#undef TP64
    seq_printf(m, "last_test_proto=0x%llx\n", atomic64_read(&priv->last_test_proto));
    seq_printf(m, "last_test_seq=%lld\n", atomic64_read(&priv->last_test_seq));
    return 0;
}

static int stage08_test_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage08_test_stats_show, inode->i_private);
}

static const struct file_operations stage08_test_stats_fops = {
    .owner = THIS_MODULE,
    .open = stage08_test_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/*
 * 【学习】debugfs queues show
 *
 * 通过 /sys/kernel/debug/netdev_stage08/queues 查看 TX/RX ring 状态
 *
 * 显示：
 * - TX/RX 的 submit_idx, notify_idx, complete_idx 等
 * - 每个 slot 的状态（FREE/POSTED/SUBMITTED/DONE）
 * - 每个 slot 关联的 skb 指针
 */
static int stage08_queues_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage08_priv *priv = netdev_priv(ndev);
    u16 i;

    seq_printf(m, "TX submit=%u notify=%u complete=%u inflight=%u done=%u\n",
           priv->txq.submit_idx, priv->txq.notify_idx, priv->txq.complete_idx,
           priv->tx_inflight, priv->tx_done);

    for (i = 0; i < min_t(u16, priv->txq.size, STAGE08_MAX_QUEUE_DUMP); ++i) {
        seq_printf(m, "  tx[%u] desc=%s slot=%s len=%u skb=%p\n",
               i,
               stage08_state_name(priv->txq.desc[i].state),
               stage08_state_name(priv->txq.slots[i].state),
               priv->txq.desc[i].data_len,
               priv->txq.slots[i].skb);
    }

    seq_printf(m, "RX post=%u device=%u consume=%u posted=%u ready=%u\n",
           priv->rxq.post_idx, priv->rxq.device_idx, priv->rxq.consume_idx,
           priv->rx_posted, priv->rx_ready);

    for (i = 0; i < min_t(u16, priv->rxq.size, STAGE08_MAX_QUEUE_DUMP); ++i) {
        seq_printf(m, "  rx[%u] desc=%s slot=%s len=%u skb=%p\n",
               i,
               stage08_state_name(priv->rxq.desc[i].state),
               stage08_state_name(priv->rxq.slots[i].state),
               priv->rxq.desc[i].data_len,
               priv->rxq.slots[i].skb);
    }
    return 0;
}

static int stage08_queues_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage08_queues_show, inode->i_private);
}

static const struct file_operations stage08_queues_fops = {
    .owner = THIS_MODULE,
    .open = stage08_queues_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/*
 * 【学习】debugfs timeline show
 *
 * 通过 /sys/kernel/debug/netdev_stage08/timeline 查看时间线
 *
 * 关键 delta：
 * - delta_submit_to_doorbell_ns：submit 到 doorbell 的延迟（应很小）
 * - delta_doorbell_to_backend_ns：doorbell 到 backend 执行的延迟（异步特征）
 * - delta_backend_to_irq_ns：backend 完成到 IRQ 的延迟
 * - delta_irq_to_poll_ns：IRQ 到 poll 的延迟
 */
static int stage08_timeline_show(struct seq_file *m, void *v)
{
    struct net_device *ndev = m->private;
    struct stage08_priv *priv = netdev_priv(ndev);

    seq_printf(m, "last_submit_ns=%llu\n", priv->timeline.last_submit_ns);
    seq_printf(m, "last_doorbell_ns=%llu\n", priv->timeline.last_doorbell_ns);
    seq_printf(m, "last_backend_wakeup_ns=%llu\n", priv->timeline.last_backend_wakeup_ns);
    seq_printf(m, "last_backend_done_ns=%llu\n", priv->timeline.last_backend_done_ns);
    seq_printf(m, "last_irq_ns=%llu\n", priv->timeline.last_irq_ns);
    seq_printf(m, "last_poll_ns=%llu\n", priv->timeline.last_poll_ns);
    seq_printf(m, "last_complete_ns=%llu\n", priv->timeline.last_complete_ns);
    seq_printf(m, "last_consume_ns=%llu\n", priv->timeline.last_consume_ns);

    if (priv->timeline.last_submit_ns && priv->timeline.last_doorbell_ns)
        seq_printf(m, "delta_submit_to_doorbell_ns=%lld\n",
               (s64)(priv->timeline.last_doorbell_ns - priv->timeline.last_submit_ns));
    if (priv->timeline.last_doorbell_ns && priv->timeline.last_backend_wakeup_ns)
        seq_printf(m, "delta_doorbell_to_backend_ns=%lld\n",
               (s64)(priv->timeline.last_backend_wakeup_ns - priv->timeline.last_doorbell_ns));
    if (priv->timeline.last_backend_done_ns && priv->timeline.last_irq_ns)
        seq_printf(m, "delta_backend_to_irq_ns=%lld\n",
               (s64)(priv->timeline.last_irq_ns - priv->timeline.last_backend_done_ns));
    if (priv->timeline.last_irq_ns && priv->timeline.last_poll_ns)
        seq_printf(m, "delta_irq_to_poll_ns=%lld\n",
               (s64)(priv->timeline.last_poll_ns - priv->timeline.last_irq_ns));
    return 0;
}

static int stage08_timeline_open(struct inode *inode, struct file *file)
{
    return single_open(file, stage08_timeline_show, inode->i_private);
}

static const struct file_operations stage08_timeline_fops = {
    .owner = THIS_MODULE,
    .open = stage08_timeline_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/*
 * 【学习】debugfs 初始化
 *
 * 创建 debugfs 目录和文件：
 * - /sys/kernel/debug/netdev_stage08/stats
 * - /sys/kernel/debug/netdev_stage08/queues
 * - /sys/kernel/debug/netdev_stage08/timeline
 *
 * 注意：debugfs 在生产环境中可能被禁用
 */
static void stage08_debugfs_init(struct stage08_priv *priv)
{
    priv->dbg_dir = debugfs_create_dir(DRV_NAME, NULL);
    if (IS_ERR_OR_NULL(priv->dbg_dir)) {
        priv->dbg_dir = NULL;
        return;
    }

    debugfs_create_file("stats", 0444, priv->dbg_dir, priv->ndev, &stage08_stats_fops);
    debugfs_create_file("test_stats", 0444, priv->dbg_dir, priv->ndev, &stage08_test_stats_fops);
    debugfs_create_file("queues", 0444, priv->dbg_dir, priv->ndev, &stage08_queues_fops);
    debugfs_create_file("timeline", 0444, priv->dbg_dir, priv->ndev, &stage08_timeline_fops);
}

/*
 * 【学习】队列清理
 *
 * stage08_cleanup_rx_queue：
 * - 遍历所有 RX slot
 * - DMA unmap
 * - 释放 skb
 * - 重置 slot 为 FREE
 *
 * stage08_cleanup_tx_queue：
 * - 同上，用于 TX slot
 *
 * 【调用时机】
 * - module exit 时
 * - alloc_queues 失败时的错误恢复
 */
static void stage08_cleanup_rx_queue(struct stage08_priv *priv)
{
    u16 i;

    for (i = 0; i < priv->rxq.size; ++i) {
        struct stage08_buf_slot *slot = &priv->rxq.slots[i];
        if (slot->skb) {
            dma_unmap_single(&priv->ndev->dev, slot->dma_addr,
                     slot->buf_len ?: priv->rx_buf_size, DMA_FROM_DEVICE);
            dev_kfree_skb_any(slot->skb);
        }
        memset(slot, 0, sizeof(*slot));
        slot->id = i;
        slot->state = S08_SLOT_FREE;
        memset(&priv->rxq.desc[i], 0, sizeof(priv->rxq.desc[i]));
        priv->rxq.desc[i].state = S08_SLOT_FREE;
    }
}

static void stage08_cleanup_tx_queue(struct stage08_priv *priv)
{
    u16 i;

    for (i = 0; i < priv->txq.size; ++i) {
        struct stage08_buf_slot *slot = &priv->txq.slots[i];
        if (slot->skb) {
            dma_unmap_single(&priv->ndev->dev, slot->dma_addr,
                     slot->data_len ?: slot->buf_len, DMA_TO_DEVICE);
            dev_kfree_skb_any(slot->skb);
        }
        memset(slot, 0, sizeof(*slot));
        slot->id = i;
        slot->state = S08_SLOT_FREE;
        memset(&priv->txq.desc[i], 0, sizeof(priv->txq.desc[i]));
        priv->txq.desc[i].state = S08_SLOT_FREE;
    }
}

/*
 * 【学习】队列分配
 *
 * 1. 分配 desc 和 slots 数组（kcalloc）
 * 2. 初始化所有 slot 为 FREE
 * 3. 预填充所有 RX slot（refill）
 *
 * 【kcalloc vs kmalloc】
 * - kcalloc：分配并清零，参数是 (nmemb, size)
 * - kmalloc：分配，可能不清零
 * - 优先使用 kcalloc，避免使用未初始化的内存
 */
static int stage08_alloc_queues(struct stage08_priv *priv)
{
    u16 i;
    int ret;

    priv->txq.desc = kcalloc(priv->txq.size, sizeof(*priv->txq.desc), GFP_KERNEL);
    priv->txq.slots = kcalloc(priv->txq.size, sizeof(*priv->txq.slots), GFP_KERNEL);
    priv->rxq.desc = kcalloc(priv->rxq.size, sizeof(*priv->rxq.desc), GFP_KERNEL);
    priv->rxq.slots = kcalloc(priv->rxq.size, sizeof(*priv->rxq.slots), GFP_KERNEL);
    if (!priv->txq.desc || !priv->txq.slots || !priv->rxq.desc || !priv->rxq.slots)
        return -ENOMEM;

    for (i = 0; i < priv->txq.size; ++i) {
        priv->txq.slots[i].id = i;
        priv->rxq.slots[i].id = i;
        priv->txq.slots[i].state = S08_SLOT_FREE;
        priv->rxq.slots[i].state = S08_SLOT_FREE;
        priv->txq.desc[i].state = S08_SLOT_FREE;
        priv->rxq.desc[i].state = S08_SLOT_FREE;
    }

    stage08_reset_queue_state(priv);

    /* 【学习】预填充 RX ring */
    for (i = 0; i < priv->rxq.size; ++i) {
        ret = stage08_post_rx_slot(priv, i);
        if (ret)
            return ret;
    }
    return 0;
}

static void stage08_free_queues(struct stage08_priv *priv)
{
    stage08_cleanup_tx_queue(priv);
    stage08_cleanup_rx_queue(priv);
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
 * 【学习】ether_setup
 *
 * 这是 Linux 提供的一个 helper 函数，
 * 用于初始化 net_device 的以太网特定字段：
 * - dev->type = ARPHRD_ETHER
 * - dev->addr_len = ETH_ALEN
 * - dev->hard_header_len = ETH_HLEN
 * - dev->min_header_len = ETH_HLEN
 * - 随机生成 MAC 地址
 *
 * 大多数以太网卡驱动都调用这个函数
 */
static void stage08_setup(struct net_device *ndev)
{
    ether_setup(ndev);
    ndev->netdev_ops = &stage08_netdev_ops;
    ndev->flags |= IFF_NOARP;
    ndev->features |= NETIF_F_HIGHDMA;
    ndev->watchdog_timeo = 5 * HZ;
    eth_hw_addr_random(ndev);
}

/*
 * 【学习】module_init / module_exit
 *
 * stage08_init：
 * 1. 参数合法性检查
 * 2. alloc_netdev 分配 net_device
 * 3. 设置 DMA capabilities
 * 4. 注册 NAPI
 * 5. 创建 backend workqueue
 * 6. 分配 TX/RX queues
 * 7. 注册 net_device
 * 8. 创建 debugfs
 *
 * stage08_exit：
 * 1. unregister_netdev
 * 2. cancel_work_sync
 * 3. destroy_workqueue
 * 4. debugfs_remove_recursive
 * 5. netif_napi_del
 * 6. free_queues
 * 7. free_netdev
 */
static int __init stage08_init(void)
{
    struct stage08_priv *priv;
    int ret;

    if (ring_size < 8)
        ring_size = 8;
    if (napi_weight < 8)
        napi_weight = 8;
    if (rx_buf_size < 256)
        rx_buf_size = 256;
    if (backend_batch < 1)
        backend_batch = 1;

    stage08_dev = alloc_netdev(sizeof(struct stage08_priv), ifname,
                   NET_NAME_UNKNOWN, stage08_setup);
    if (!stage08_dev)
        return -ENOMEM;

    priv = netdev_priv(stage08_dev);
    priv->ndev = stage08_dev;
    priv->rx_buf_size = rx_buf_size;
    priv->backend_delay_us = backend_delay_us;
    priv->backend_batch = backend_batch;
    priv->txq.size = ring_size;
    priv->rxq.size = ring_size;
    spin_lock_init(&priv->state_lock);

    ret = stage08_prepare_dma_caps(stage08_dev);
    if (ret)
        netdev_warn(stage08_dev, "dma_set_mask_and_coherent failed: %d\n", ret);

    STAGE08_NETIF_NAPI_ADD(stage08_dev, &priv->napi, stage08_poll, napi_weight);

    /* 【学习】alloc_ordered_workqueue
     *
     * WQ_MEM_RECLAIM：如果内存紧张，确保 workqueue 不死锁
     * ordered：保证同一时刻只有一项在执行
     */
    priv->backend_wq = alloc_ordered_workqueue("stage08_backend", WQ_MEM_RECLAIM);
    if (!priv->backend_wq) {
        ret = -ENOMEM;
        goto err_napi;
    }
    INIT_WORK(&priv->backend_work, stage08_backend_workfn);

    ret = stage08_alloc_queues(priv);
    if (ret)
        goto err_wq;

    ret = register_netdev(stage08_dev);
    if (ret)
        goto err_queue;

    stage08_debugfs_init(priv);
    pr_info("stage08 loaded: ifname=%s ring=%d napi=%d rx_buf=%d backend_delay_us=%d batch=%d\n",
        stage08_dev->name, ring_size, napi_weight, rx_buf_size, backend_delay_us, backend_batch);
    return 0;

err_queue:
    stage08_free_queues(priv);
err_wq:
    destroy_workqueue(priv->backend_wq);
    priv->backend_wq = NULL;
err_napi:
    netif_napi_del(&priv->napi);
    free_netdev(stage08_dev);
    stage08_dev = NULL;
    return ret;
}

static void __exit stage08_exit(void)
{
    struct stage08_priv *priv;

    if (!stage08_dev)
        return;

    priv = netdev_priv(stage08_dev);
    unregister_netdev(stage08_dev);
    cancel_work_sync(&priv->backend_work);
    if (priv->backend_wq)
        destroy_workqueue(priv->backend_wq);
    debugfs_remove_recursive(priv->dbg_dir);
    netif_napi_del(&priv->napi);
    stage08_free_queues(priv);
    free_netdev(stage08_dev);
    stage08_dev = NULL;
    pr_info("stage08 unloaded\n");
}

module_init(stage08_init);
module_exit(stage08_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI + user project");
MODULE_DESCRIPTION("stage08 async backend transport v0/v1");