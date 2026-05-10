// SPDX-License-Identifier: GPL-2.0
/*
 * af_xdp_forwarder_kern.bpf.c — AF_XDP 转发器内核侧 XDP program
 *
 * 功能：
 *   1. 根据 rx_queue_index 查找 XSKMAP 中对应的 socket fd
 *   2. 若存在 socket，bpf_redirect_map 跳转到 AF_XDP socket
 *   3. 若不存在，走 XDP_PASS 送到内核协议栈
 *
 * 与 lab-af-xdp-socket-rings 的差异：
 *   本程序不区分 DROP/PASS 动作，所有处理都由用户态决定。
 *   这里只负责把包 redirect 到 socket，forwarder 的 drop/reflect 策略
 *   由用户态 af_xdp_forwarder.c 在 RX ring 取包后执行。
 *
 * 统计：
 *   stats_map（PERCPU_ARRAY）记录：
 *     STAT_PASS           — 队列号超出 MAX_XSKS，直接 PASS
 *     STAT_REDIRECT       — 成功 redirect 到 socket
 *     STAT_REDIRECT_MISS  — XSKMAP 中该队列无 socket
 */
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#define MAX_XSKS 64

/* 统计索引（与 stats_map 的 entry 对应）*/
#define STAT_PASS 0            // 队列号越界，直接 PASS
#define STAT_REDIRECT 1       // 成功 redirect 到 AF_XDP socket
#define STAT_REDIRECT_MISS 2  // XSKMAP 中该队列无 socket
#define STAT_MAX 3

/* action_stats — per-CPU 统计结构
 *
 * 使用 PERCPU_ARRAY：每个 CPU 独立计数，不需要原子操作或锁。
 */
struct action_stats {
    __u64 packets;            // 处理的数据包数
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
 */
struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, MAX_XSKS);
    __type(key, __u32);
    __type(value, __u32);
} xsks_map SEC(".maps");

/* stats_map：XDP action 统计（per-CPU）*/
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, STAT_MAX);
    __type(key, __u32);
    __type(value, struct action_stats);
} stats_map SEC(".maps");

// bump_stat — 增加对应 action 的统计计数
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
//   1. 取 data_end 和 data，计算数据包大小
//   2. 取 rx_queue_index 作为 XSKMAP lookup 的 key
//   3. 越界检查（qid >= MAX_XSKS → XDP_PASS）
//   4. 查 XSKMAP：有 socket → redirect；无 socket → XDP_PASS
//
// 注意：这里的 bpf_redirect_map 跳转目标是 AF_XDP socket，
// 不是另一个网口。数据包从网口进入内核后，直接送到用户态程序。
SEC("xdp")
int xdp_sock_prog(struct xdp_md *ctx)
{
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;
    __u64 bytes = data_end > data ? (__u64)(data_end - data) : 0;
    __u32 qid = ctx->rx_queue_index;

    // 队列号越界，不处理
    if (qid >= MAX_XSKS) {
        bump_stat(STAT_PASS, bytes);
        return XDP_PASS;
    }

    // 查 XSKMAP，看该队列是否注册了 AF_XDP socket
    if (bpf_map_lookup_elem(&xsks_map, &qid)) {
        bump_stat(STAT_REDIRECT, bytes);
        // redirect 到 socket 后，数据包进入 AF_XDP 的 RX ring
        // 用户态程序从 RX ring 取包后，执行 drop 或 reflect 策略
        return bpf_redirect_map(&xsks_map, qid, XDP_PASS);
    }

    // XSKMAP 中该队列无 socket，送到内核协议栈
    bump_stat(STAT_REDIRECT_MISS, bytes);
    return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";