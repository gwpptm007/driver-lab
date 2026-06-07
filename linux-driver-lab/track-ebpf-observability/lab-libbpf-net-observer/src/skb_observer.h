/* SPDX-License-Identifier: GPL-2.0 */
/*
 * skb_observer.h — 共享定义，BPF 内核程序和 userspace 共同引用
 *
 * Phase 4: 从 bpftrace 脚本迁移到 C/libbpf 编译型观测工具。
 * 本文件定义 ringbuf 事件结构、事件类型枚举、配置常量。
 */

#ifndef __SKB_OBSERVER_H
#define __SKB_OBSERVER_H

/* === ringbuf 事件类型 ===
 *
 * 每个 tracepoint 触发时，BPF 程序填充一个 skb_event 并通过
 * ringbuf 提交给 userspace。type 字段区分事件来源。
 */
enum skb_event_type {
	EVENT_RX = 0,       /* net:netif_receive_skb        — skb 进入协议栈 */
	EVENT_GRO = 1,      /* net:napi_gro_receive_entry    — GRO 合包入口   */
	EVENT_TX_QUEUE = 2,  /* net:net_dev_queue             — skb 入发送队列  */
	EVENT_TX_XMIT = 3,   /* net:net_dev_start_xmit        — 驱动开始发送   */
	EVENT_DROP = 4,      /* skb:kfree_skb                 — skb 释放/drop  */
	EVENT_MAX,
};

/* === ringbuf 事件结构 ===
 *
 * padding 到 64 字节对齐，利于 cache line。
 * 每次 tracepoint 触发推一个事件到 ringbuf。
 */
struct skb_event {
	unsigned int type;        /* enum skb_event_type */
	unsigned int cpu;         /* 触发事件的 CPU */
	unsigned int len;         /* 包长度 (部分 tracepoint 提供) */
	unsigned int ifindex;     /* 网络接口索引 */
	unsigned long long timestamp;  /* bpf_ktime_get_ns() 单调时间戳 */
	char ifname[16];          /* 接口名称 (IFNAMSIZ=16) */
};

/* 默认 ringbuf 大小: 2MB，足够高流量场景 */
#define RINGBUF_SIZE (2 * 1024 * 1024)

/* 最大观测接口数，防止 map 无限增长 */
#define MAX_IFACES 32

#endif /* __SKB_OBSERVER_H */
