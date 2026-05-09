// SPDX-License-Identifier: GPL-2.0
/*
 * af_xdp_kern.bpf.c — AF_XDP 内核侧 XDP program
 *
 * 功能：
 *   1. 根据 rx_queue_index 查找 XSKMAP 中对应的 socket fd
 *   2. 若存在 socket，bpf_redirect_map 跳转到 AF_XDP socket
 *   3. 若不存在，走 XDP_PASS 送到内核协议栈
 *
 * 统计：
 *   stats_map（PERCPU_ARRAY）记录四类动作的 packet 数和 byte 数：
 *     STAT_PASS            — 未命中 XSKMAP，直接 PASS
 *     STAT_REDIRECT       — 成功 redirect 到 socket
 *     STAT_REDIRECT_MISS  — 查了 XSKMAP 但该队列没有注册 socket
 *     STAT_ABORTED        — （保留，当前未用到）
 *
 * XSKMAP：
 *   XSKMAP 的 key 是 queue_id，value 是 socket fd。
 *   BPF program 查 xsks_map[rx_queue_index] 确认是否需要 redirect。
 *   用户态在启动时通过 bpf_map_update_elem 把 socket fd 写入 XSKMAP。
 */
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#define MAX_XSKS 64

/* 统计索引（与 stats_map 的 entry 对应）*/
#define STAT_PASS 0            // 未命中 XSKMAP，直接 PASS
#define STAT_REDIRECT 1        // 成功 redirect 到 AF_XDP socket
#define STAT_REDIRECT_MISS 2   // XSKMAP 该队列无 socket，转 PASS
#define STAT_ABORTED 3         // 保留（当前未使用）
#define STAT_MAX 4

/* action_stats — per-CPU 统计结构
 *
 * 使用 PERCPU_ARRAY 类型：
 *   - 每个 CPU 独立计数，不需要原子操作或锁
 *   - 读取时需要用 bpf_percpu_array_read 或直接 map lookup
 *
 * 注意：字节统计以 data_end - data 为准，是 XDP 处理时的数据包大小。
 */
struct action_stats {
    __u64 packets;             // 处理的数据包数
    __u64 bytes;               // 处理的总字节数
};

/* XSKMAP：AF_XDP socket 的 BPF map
 *
 * 类型：BPF_MAP_TYPE_XSKMAP
 * key  ：u32（队列号）
 * value：u32（AF_XDP socket fd）
 * max_entries：最多支持 MAX_XSKS 个队列
 *
 * 用户态通过 bpf_map_update_elem(xsks_fd, &queue_id, &socket_fd, 0) 注册。
 * 内核侧通过 bpf_map_lookup_elem(&xsks_map, &qid) 查询。
 */
struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, MAX_XSKS);
    __type(key, __u32);
    __type(value, __u32);
} xsks_map SEC(".maps");

/* stats_map：XDP action 统计
 *
 * 类型：BPF_MAP_TYPE_PERCPU_ARRAY
 * 每个 CPU 独立计数（无锁），适合多核场景。
 * entry 0=STAT_PASS, 1=STAT_REDIRECT, 2=STAT_REDIRECT_MISS, 3=STAT_ABORTED。
 */
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, STAT_MAX);
    __type(key, __u32);
    __type(value, struct action_stats);
} stats_map SEC(".maps");

// bump_stat — 增加对应 action 的统计计数
//
// 注意：bpf_map_lookup_elem 返回的是当前 CPU 的 per-CPU 数据，
// 不需要额外的锁或原子操作。
static __always_inline void bump_stat(__u32 key, __u64 bytes)
{
    struct action_stats *s = bpf_map_lookup_elem(&stats_map, &key);
    if (!s)
        return;
    s->packets++;
    s->bytes += bytes;
}

// xdp_sock_prog — XDP program 主函数
//
// 处理流程：
//   1. 提取 data_end 和 data，计算数据包大小
//   2. 取 rx_queue_index 作为 XSKMAP lookup 的 key
//   3. 越界检查（queue_id >= MAX_XSKS → 直接 PASS）
//   4. 查 XSKMAP：有 socket → redirect；无 socket → PASS
//
// XDP 处理阶段：
//   - 此时还没有分配 skb，是最早的数据包处理点
//   - bpf_redirect_map 是同步的，数据包直接进入 AF_XDP socket 的 RX ring
SEC("xdp")
int xdp_sock_prog(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    // 防御：data_end >= data 时才有有效数据包
    __u64 bytes = data_end > data ? (__u64)(data_end - data) : 0;
    __u32 qid = ctx->rx_queue_index;

    // 队列号越界：超过 MAX_XSKS 的队列不处理
    if (qid >= MAX_XSKS) {
        bump_stat(STAT_PASS, bytes);
        return XDP_PASS;
    }

    // 查 XSKMAP，看该队列是否注册了 AF_XDP socket
    if (bpf_map_lookup_elem(&xsks_map, &qid)) {
        // 找到了 socket fd，执行 redirect
        // XDP_PASS 是 fallback：若 redirect 失败（如 socket 满了），走 PASS
        bump_stat(STAT_REDIRECT, bytes);
        return bpf_redirect_map(&xsks_map, qid, XDP_PASS);
    }

    // XSKMAP 中该队列无 socket，转发到内核协议栈
    bump_stat(STAT_REDIRECT_MISS, bytes);
    return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";