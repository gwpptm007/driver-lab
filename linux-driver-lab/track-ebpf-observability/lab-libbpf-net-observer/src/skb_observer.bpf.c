// SPDX-License-Identifier: GPL-2.0
/*
 * skb_observer.bpf.c — BPF 内核端代码
 *
 * Phase 4: 从 bpftrace 脚本迁移到 C/libbpf 编译型观测工具。
 *
 * 观测对象 (与 Phase 3 bpftrace 脚本完全对应):
 *   net:netif_receive_skb       → EVENT_RX
 *   net:napi_gro_receive_entry   → EVENT_GRO
 *   net:net_dev_queue            → EVENT_TX_QUEUE
 *   net:net_dev_start_xmit       → EVENT_TX_XMIT
 *   skb:kfree_skb                → EVENT_DROP
 *
 * 关键设计:
 *   1. tracepoint context 结构手动定义 (内核 ABI 保证跨版本不变)
 *   2. ringbuf 推事件给 userspace
 *   3. per-CPU / per-event 统计 maps
 *   4. CO-RE: 使用 vmlinux.h + BPF_CORE_READ (仅用于 skb 内部字段访问)
 *
 * tracepoint context 布局来源: /sys/kernel/.../events/net/<name>/format
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>

#include "skb_observer.h"

char LICENSE[] SEC("license") = "GPL";

/* ================================================================
 * 手动定义的 tracepoint context 结构
 *
 * 这些结构不在 vmlinux.h 中 (TRACE_EVENT 宏生成的结构体不被 BTF 捕获)。
 * 但它们属于内核 tracepoint ABI，跨版本稳定。
 *
 * 每个 tracepoint context = trace_entry(8bytes) + TP_STRUCT__entry 的字段
 * __data_loc 字段存储为 u32 (低 16bit=offset, 高 16bit=length)
 * ================================================================ */

/*
 * tracepoint context 结构 — 手动匹配内核 ABI 布局
 *
 * 布局来源: /sys/kernel/debug/tracing/events/<subsys>/<name>/format
 * 格式: common fields (8 bytes) + tracepoint-specific fields
 *
 * common fields =
 *   common_type        u16 offset:0
 *   common_flags       u8  offset:2
 *   common_preempt_count u8 offset:3
 *   common_pid         s32 offset:4
 */

/* netif_receive_skb: skbaddr(8) + len(16) + name(20), total=24 */
struct tp_netif_receive_skb {
	__u64 __trace_entry;    /* common fields */
	__u64 skbaddr;          /* void *skbaddr   offset:8 */
	__u32 len;              /* unsigned int len offset:16 */
	__u32 name;             /* __data_loc char[] name  offset:20 */
};

/* net_dev_queue: 与 netif_receive_skb 布局相同 */
struct tp_net_dev_queue {
	__u64 __trace_entry;
	__u64 skbaddr;          /* offset:8 */
	__u32 len;              /* offset:16 */
	__u32 name;             /* offset:20 */
};

/* napi_gro_receive_entry: name(8) + napi_id(12) + ..., 仅访问 name */
struct tp_napi_gro_receive_entry {
	__u64 __trace_entry;
	__u32 name;             /* __data_loc char[] name  offset:8 */
	__u32 __pad;            /* 确保 struct >= 12 bytes */
};

/* net_dev_start_xmit: name(8) → ... → len(36) */
struct tp_net_dev_start_xmit {
	__u64 __trace_entry;    /* offset:0 */
	__u32 name;             /* __data_loc char[] name  offset:8 */
	__u16 queue_mapping;    /* offset:12 */
	__u16 __pad1;           /* offset:14 (align) */
	__u64 skbaddr;          /* offset:16 */
	bool   vlan_tagged;     /* offset:24 */
	__u16  vlan_proto;      /* offset:26 */
	__u16  vlan_tci;        /* offset:28 */
	__u16  protocol;        /* offset:30 */
	__u8   ip_summed;       /* offset:32 */
	__u8   __pad2[3];       /* offset:33 (align to 4) */
	__u32  len;             /* offset:36 */
};

/* kfree_skb: handler 不访问任何字段, 最小 struct 即可 */
struct tp_kfree_skb {
	__u64 __trace_entry;
};

/* ================================================================
 * 辅助: 从 __data_loc 读取字符串
 *
 * __data_loc 编码: u32 = (length << 16) | (offset & 0xFFFF)
 * 字符串: ctx_base + offset (offset 相对于 trace entry 起始)
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

	/* __data_loc offset 是相对于 trace entry 起始 (ctx_base)，不是字段本身 */
	if (offset > 0 && length > 0)
		bpf_probe_read_kernel_str(out, out_len,
					  (const char *)ctx_base + offset);
}

/* ================================================================
 * ringbuf: 事件队列，userspace 消费
 * ================================================================ */
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, RINGBUF_SIZE);
} events SEC(".maps");

/* ================================================================
 * 统计 maps
 * ================================================================ */
struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, EVENT_MAX);
	__type(key, __u32);
	__type(value, __u64);
} event_counts SEC(".maps");

/* ================================================================
 * 辅助: 提交事件到 ringbuf
 * ================================================================ */
static __always_inline int submit_event(__u32 type, __u32 len,
					 const char *ifname)
{
	struct skb_event *ev;

	ev = bpf_ringbuf_reserve(&events, sizeof(*ev), 0);
	if (!ev)
		return 0;

	ev->type = type;
	ev->cpu = bpf_get_smp_processor_id();
	ev->len = len;
	ev->ifindex = 0;
	ev->timestamp = bpf_ktime_get_ns();

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
 * tracepoint handlers
 * ================================================================ */

SEC("tracepoint/net/netif_receive_skb")
int tp_netif_receive_skb(struct tp_netif_receive_skb *ctx)
{
	char ifname[16];
	read_tp_name(ctx, &ctx->name, ifname, sizeof(ifname));
	submit_event(EVENT_RX, ctx->len, ifname);
	return 0;
}

SEC("tracepoint/net/napi_gro_receive_entry")
int tp_napi_gro_receive_entry(struct tp_napi_gro_receive_entry *ctx)
{
	char ifname[16];
	read_tp_name(ctx, &ctx->name, ifname, sizeof(ifname));
	submit_event(EVENT_GRO, 0, ifname);
	return 0;
}

SEC("tracepoint/net/net_dev_queue")
int tp_net_dev_queue(struct tp_net_dev_queue *ctx)
{
	char ifname[16];
	read_tp_name(ctx, &ctx->name, ifname, sizeof(ifname));
	submit_event(EVENT_TX_QUEUE, ctx->len, ifname);
	return 0;
}

SEC("tracepoint/net/net_dev_start_xmit")
int tp_net_dev_start_xmit(struct tp_net_dev_start_xmit *ctx)
{
	char ifname[16];
	read_tp_name(ctx, &ctx->name, ifname, sizeof(ifname));
	submit_event(EVENT_TX_XMIT, ctx->len, ifname);
	return 0;
}

SEC("tracepoint/skb/kfree_skb")
int tp_kfree_skb(struct tp_kfree_skb *ctx)
{
	/* kfree_skb 没有 name 字段。ifname 留空。 */
	{
		char empty[16] = {0};
		submit_event(EVENT_DROP, 0, empty);
	}
	return 0;
}
