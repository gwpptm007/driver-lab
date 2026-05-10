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
#define STAT_PASS 0
#define STAT_REDIRECT 1
#define STAT_REDIRECT_MISS 2
#define STAT_ABORTED 3
#define STAT_MAX 4

struct action_stats {
    __u64 packets;
    __u64 bytes;
};

struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, MAX_XSKS);
    __type(key, __u32);
    __type(value, __u32);
} xsks_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, STAT_MAX);
    __type(key, __u32);
    __type(value, struct action_stats);
} stats_map SEC(".maps");

static __always_inline void bump_stat(__u32 key, __u64 bytes)
{
    struct action_stats *s = bpf_map_lookup_elem(&stats_map, &key);
    if (!s)
        return;
    s->packets++;
    s->bytes += bytes;
}

SEC("xdp")
int xdp_sock_prog(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    __u64 bytes = data_end > data ? (__u64)(data_end - data) : 0;
    __u32 qid = ctx->rx_queue_index;

    if (qid >= MAX_XSKS) {
        bump_stat(STAT_PASS, bytes);
        return XDP_PASS;
    }

    if (bpf_map_lookup_elem(&xsks_map, &qid)) {
        bump_stat(STAT_REDIRECT, bytes);
        return bpf_redirect_map(&xsks_map, qid, XDP_PASS);
    }

    bump_stat(STAT_REDIRECT_MISS, bytes);
    return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";
