// SPDX-License-Identifier: GPL-2.0
/*
 * AF_XDP socket rings lab - kernel/XDP side
 *
 * This program redirects packets from an RX queue into an AF_XDP socket
 * registered in xsks_map[rx_queue_index]. If no socket is registered for the
 * queue, packets are passed to the kernel stack.
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
