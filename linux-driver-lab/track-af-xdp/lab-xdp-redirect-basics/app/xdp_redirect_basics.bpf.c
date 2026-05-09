// SPDX-License-Identifier: GPL-2.0
/*
 * xdp_redirect_basics.bpf.c
 *
 * AF_XDP Track Phase 1：XDP 基础实验
 *
 * 本程序实现：
 *   1. 可配置的 XDP action（PASS / DROP / REDIRECT）
 *   2. per-CPU action 统计（packets / bytes）
 *   3. XSKMAP 为下一站 AF_XDP socket 留好接口
 *
 * XDP 程序运行在内核态（在驱动层，比 skb 分配更早），
 * 因此无法调用任意 libc 函数，只能使用 bpf_* helpers。
 */
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

/*============================================================
 * 数据结构定义
 *============================================================*/

/*
 * 每种 action 的统计计数器
 * packets: 处理了多少个包
 * bytes:   处理了多少字节
 */
struct xdp_action_stat {
    __u64 packets;
    __u64 bytes;
};

/*============================================================
 * Map 定义（三张 map）
 *============================================================*/

/*
 * config_map — 用户态写入，BPF 程序读取
 * 用途：控制 XDP 程序的行为（PASS / DROP / REDIRECT）
 * 类型：ARRAY（索引 0 存一个 uint32_t action）
 */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);         // 只有一个配置项：config_map[0]
    __type(key, __u32);
    __type(value, __u32);           // value = action (XDP_PASS/DROP/REDIRECT)
} config_map SEC(".maps");

/*
 * stats_map — BPF 程序写入，用户态读取
 * 用途：统计每种 action 处理了多少包和字节
 * 类型：PERCPU_ARRAY（每 CPU 独立计数，避免加锁）
 */
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 8);         // 8 个桶，分别统计 DROP/PASS/REDIRECT
    __type(key, __u32);
    __type(value, struct xdp_action_stat);
} stats_map SEC(".maps");

/*
 * xsks_map — 下一站 AF_XDP socket fd 写入这里
 * 用途：XDP REDIRECT 目标
 * XDP 程序通过 bpf_redirect_map(&xsks_map, queue_id) 把包重定向到 AF_XDP socket
 *
 * 注意：本 lab 还没有创建 AF_XDP socket，所以 REDIRECT 只是 dry-run
 */
struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, 64);        // 最大 64 个队列
    __type(key, __u32);             // key = 队列 ID
    __type(value, __u32);           // value = AF_XDP socket fd
} xsks_map SEC(".maps");

/*============================================================
 * 辅助函数
 *============================================================*/

/*
 * count_action — 更新 stats_map
 * action: XDP_DROP / XDP_PASS / XDP_REDIRECT
 * bytes:  当前包的字节数
 *
 * 为什么用 per-CPU map？
 * 多核 CPU 下，每个核有独立的统计副本，累加时不需要加锁
 * 用户态读取时把所有 CPU 的值加起来即可
 */
static __always_inline void count_action(__u32 action, __u64 bytes)
{
    __u32 key = action;

    // 安全检查：action 最大为 7（XDP_REDIRECT=4），超过则归到 key=0
    if (key >= 8)
        key = 0;

    // 查 stats_map[key]（per-CPU array 的 lookup）
    struct xdp_action_stat *stat = bpf_map_lookup_elem(&stats_map, &key);
    if (!stat)
        return;                       // map 里没有这一项就跳过

    // 原子递增（每核独立，不需要锁）
    stat->packets += 1;
    stat->bytes += bytes;
}

/*============================================================
 * XDP 主程序（SEC("xdp") 标记是 XDP hook）
 *============================================================*/

SEC("xdp")
int xdp_redirect_basics(struct xdp_md *ctx)
{
    __u32 cfg_key = 0;              // config_map[0] 存 action
    __u32 action = XDP_PASS;        // 默认是 PASS
    __u32 *configured_action;
    /*
     * 计算包长度：
     *   ctx->data     → Ethernet 头起始
     *   ctx->data_end → 包的结束位置
     *   data_end - data = 包长（字节）
     *
     * 注意：XDP 阶段还没有 skb，所以用 xdp_md 里的指针计算
     */
    __u64 bytes = (__u64)ctx->data_end - (__u64)ctx->data;

    /*----------------------------------------
     * 1. 查 config_map，看用户态配置了什么 action
     *----------------------------------------*/
    configured_action = bpf_map_lookup_elem(&config_map, &cfg_key);
    if (configured_action)
        action = *configured_action;  // 用户态配置优先

    /*----------------------------------------
     * 2. 根据 action 执行对应逻辑
     *----------------------------------------*/
    if (action == XDP_DROP) {
        // 统计 + 直接丢弃（不分配 skb，性能最高）
        count_action(XDP_DROP, bytes);
        return XDP_DROP;
    }

    if (action == XDP_REDIRECT) {
        /*
         * REDIRECT 到 XSKMAP 中对应队列的 socket
         * ctx->rx_queue_index：收到包的队列 ID（用于 lookup）
         * 第三个参数 XDP_PASS：lookup 失败时返回 XDP_PASS（而不是 DROP）
         *
         * 注意：当前 xsks_map 为空（没有 AF_XDP socket），
         *       所以 lookup 失败会走 XDP_PASS，即放行到内核。
         *       这只是验证 redirect 代码路径，实际 AF_XDP 在下一站。
         */
        count_action(XDP_REDIRECT, bytes);
        return bpf_redirect_map(&xsks_map, ctx->rx_queue_index, XDP_PASS);
    }

    /*----------------------------------------
     * 3. 默认：XDP_PASS（放行到内核协议栈）
     *----------------------------------------*/
    count_action(XDP_PASS, bytes);
    return XDP_PASS;
}

/*
 * 许可证声明（必须）
 * 只有 GPL 许可证才能使用 GPL-only 的 BPF helpers
 */
char LICENSE[] SEC("license") = "GPL";
