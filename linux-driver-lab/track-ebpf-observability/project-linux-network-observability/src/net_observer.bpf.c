// SPDX-License-Identifier: GPL-2.0
/*
 * net_observer.bpf.c — BPF 内核端代码
 *
 * Phase 5: 整合 Phase 1-4 的观测能力，形成统一的网络可观测性项目工具。
 *
 * 观测对象 (5 个内核 tracepoint):
 *   net:netif_receive_skb       → EVENT_RX
 *   net:napi_gro_receive_entry   → EVENT_GRO
 *   net:net_dev_queue            → EVENT_TX_QUEUE
 *   net:net_dev_start_xmit       → EVENT_TX_XMIT
 *   skb:kfree_skb                → EVENT_DROP (含 drop_reason)
 *
 * 关键设计:
 *   1. tracepoint context 手工匹配内核 ABI (/sys/.../events/.../format)
 *   2. ringbuf 推送事件到 userspace
 *   3. per-CPU per-event-type 统计 map
 *   4. kfree_skb 额外捕获 protocol 和 drop_reason
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>

#include "net_observer.h"

char LICENSE[] SEC("license") = "GPL";

/* ================================================================
 * tracepoint context 结构 — 手工匹配内核 ABI
 *
 * 布局来源: /sys/kernel/debug/tracing/events/<subsys>/<name>/format
 *
 * common fields (每个 tracepoint 前 8 字节):
 *   common_type           u16 offset:0
 *   common_flags          u8  offset:2
 *   common_preempt_count  u8  offset:3
 *   common_pid            s32 offset:4
 * ================================================================ */

/* netif_receive_skb: skbaddr(8) + len(16) + name(20), total=24 */
struct tp_netif_receive_skb {
	__u64 __pad0;           /* common fields */
	__u64 skbaddr;          /* offset:8  void *skbaddr */
	__u32 len;              /* offset:16 unsigned int len */
	__u32 name;             /* offset:20 __data_loc char[] name */
};

/* net_dev_queue: 与 netif_receive_skb 布局完全相同 */
struct tp_net_dev_queue {
	__u64 __pad0;
	__u64 skbaddr;          /* offset:8 */
	__u32 len;              /* offset:16 */
	__u32 name;             /* offset:20 */
};

/* napi_gro_receive_entry: name(8) + napi_id(12) + ... */
struct tp_napi_gro_receive_entry {
	__u64 __pad0;
	__u32 name;             /* offset:8  __data_loc char[] name */
	__u32 __pad1;           /* offset:12 napi_id (unused) */
};

/* net_dev_start_xmit: name(8) → ... → len(36) */
struct tp_net_dev_start_xmit {
	__u64 __pad0;           /* offset:0  common fields */
	__u32 name;             /* offset:8  __data_loc char[] name */
	__u16 queue_mapping;    /* offset:12 */
	__u16 __pad1;           /* offset:14 */
	__u64 skbaddr;          /* offset:16 */
	bool   vlan_tagged;     /* offset:24 */
	__u16  vlan_proto;      /* offset:26 */
	__u16  vlan_tci;        /* offset:28 */
	__u16  protocol;        /* offset:30 */
	__u8   ip_summed;       /* offset:32 */
	__u8   __pad2[3];       /* offset:33 */
	__u32  len;             /* offset:36 */
};

/* kfree_skb: skbaddr(8) + location(16) + protocol(24) + reason(28) */
struct tp_kfree_skb {
	__u64 __pad0;           /* offset:0  common fields */
	__u64 skbaddr;          /* offset:8  void *skbaddr */
	__u64 location;         /* offset:16 void *location (drop location) */
	__u16 protocol;         /* offset:24 unsigned short protocol */
	__u16 __pad1;           /* offset:26 (alignment) */
	__u32 reason;           /* offset:28 enum skb_drop_reason */
};

/* ================================================================
 * 辅助: 从 __data_loc 读取接口名称
 *
 * __data_loc 编码: u32 = (length << 16) | (offset & 0xFFFF)
 * 字符串位于: ctx_base + offset (offset 相对于 trace entry 起始)
 * ================================================================ */
static __always_inline void read_tp_name(const void *ctx_base,
					  const void *name_field,
					  char *out, int out_len)
{
	__u32 data_loc;
	bpf_probe_read_kernel(&data_loc, sizeof(data_loc), name_field);

	__u16 offset = data_loc & 0xFFFF;
	__u16 length = data_loc >> 16;
	if (length >= out_len)
		length = out_len - 1;

	if (offset > 0 && length > 0)
		bpf_probe_read_kernel_str(out, out_len,
					  (const char *)ctx_base + offset);
}

/* ================================================================
 * ringbuf: 事件队列
 * ================================================================ */
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, RINGBUF_SIZE);
} events SEC(".maps");

/* ================================================================
 * 统计: per-CPU per-event-type 计数
 * key = enum net_event_type (0-4), value = per-CPU u64 数组
 * ================================================================ */
struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, EVENT_MAX);
	__type(key, __u32);
	__type(value, __u64);
} event_counts SEC(".maps");

/* ================================================================
 * 辅助: 提交事件到 ringbuf + 更新统计
 * ================================================================ */
static __always_inline int submit_event(__u32 type, __u32 len,
					 const char *ifname,
					 __u16 protocol, __u16 drop_reason)
{
	struct net_event *ev;

	ev = bpf_ringbuf_reserve(&events, sizeof(*ev), 0);
	if (!ev)
		return 0;

	ev->type = type;
	ev->cpu = bpf_get_smp_processor_id();
	ev->len = len;
	ev->ifindex = 0;
	ev->timestamp = bpf_ktime_get_ns();
	ev->protocol = protocol;
	ev->drop_reason = drop_reason;

	if (ifname)
		__builtin_memcpy(ev->ifname, ifname, sizeof(ev->ifname));
	else
		__builtin_memset(ev->ifname, 0, sizeof(ev->ifname));

	bpf_ringbuf_submit(ev, 0);

	/* 更新 per-event 计数 */
	__u32 key = type;
	__u64 *val = bpf_map_lookup_elem(&event_counts, &key);
	if (val)
		__sync_fetch_and_add(val, 1);

	return 0;
}

/* ================================================================
 * tracepoint handlers (5 个)
 * ================================================================ */

SEC("tracepoint/net/netif_receive_skb")
int tp_netif_receive_skb(struct tp_netif_receive_skb *ctx)
{
	char ifname[16] = {};
	read_tp_name(ctx, &ctx->name, ifname, sizeof(ifname));
	submit_event(EVENT_RX, ctx->len, ifname, 0, 0);
	return 0;
}

SEC("tracepoint/net/napi_gro_receive_entry")
int tp_napi_gro_receive_entry(struct tp_napi_gro_receive_entry *ctx)
{
	char ifname[16] = {};
	read_tp_name(ctx, &ctx->name, ifname, sizeof(ifname));
	submit_event(EVENT_GRO, 0, ifname, 0, 0);
	return 0;
}

SEC("tracepoint/net/net_dev_queue")
int tp_net_dev_queue(struct tp_net_dev_queue *ctx)
{
	char ifname[16] = {};
	read_tp_name(ctx, &ctx->name, ifname, sizeof(ifname));
	submit_event(EVENT_TX_QUEUE, ctx->len, ifname, 0, 0);
	return 0;
}

SEC("tracepoint/net/net_dev_start_xmit")
int tp_net_dev_start_xmit(struct tp_net_dev_start_xmit *ctx)
{
	char ifname[16] = {};
	read_tp_name(ctx, &ctx->name, ifname, sizeof(ifname));
	submit_event(EVENT_TX_XMIT, ctx->len, ifname, 0, 0);
	return 0;
}

SEC("tracepoint/skb/kfree_skb")
int tp_kfree_skb(struct tp_kfree_skb *ctx)
{
	__u16 proto = ctx->protocol;
	__u32 reason = ctx->reason;
	submit_event(EVENT_DROP, 0, NULL, proto, (__u16)reason);
	return 0;
}
