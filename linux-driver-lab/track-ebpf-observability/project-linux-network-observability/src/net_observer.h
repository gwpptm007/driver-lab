/* SPDX-License-Identifier: GPL-2.0 */
/*
 * net_observer.h — 共享定义
 *
 * Phase 5: 整合 Phase 1-4 的观测能力，形成统一的网络可观测性项目工具。
 * 本文件定义 ringbuf 事件结构、事件类型、统计结构和配置常量。
 */

#ifndef __NET_OBSERVER_H
#define __NET_OBSERVER_H

/* === 事件类型 === */
enum net_event_type {
	EVENT_RX        = 0,  /* net:netif_receive_skb     — skb 进入协议栈 */
	EVENT_GRO       = 1,  /* net:napi_gro_receive_entry — GRO 合包入口   */
	EVENT_TX_QUEUE  = 2,  /* net:net_dev_queue          — skb 入发送队列  */
	EVENT_TX_XMIT   = 3,  /* net:net_dev_start_xmit     — 驱动开始发送   */
	EVENT_DROP      = 4,  /* skb:kfree_skb              — skb 释放/drop  */
	EVENT_MAX,
};

/* === ringbuf 事件结构 === */
struct net_event {
	__u32 type;           /* enum net_event_type */
	__u32 cpu;            /* 触发 CPU */
	__u32 len;            /* 包长度 (L2) */
	__u32 ifindex;        /* 网络接口索引 (0 = 未知) */
	__u64 timestamp;      /* bpf_ktime_get_ns() */
	__u16 protocol;       /* L3 协议 (ETH_P_IP=0x0800, 仅 DROP 事件) */
	__u16 drop_reason;    /* enum skb_drop_reason (仅 DROP 事件) */
	char  ifname[16];     /* 接口名称 (IFNAMSIZ=16) */
};

/* ringbuf 大小: 2MB */
#define RINGBUF_SIZE (2 * 1024 * 1024)

/* ================================================================
 * drop_reason 常见值 (内核 enum skb_drop_reason 子集)
 *
 * 来源: include/linux/dropreason.h
 * 仅列出网络路径中最常见的几种。
 * ================================================================ */
#define DROP_NOT_SPECIFIED      2
#define DROP_NO_SOCKET          3
#define DROP_PKT_TOO_SMALL      4
#define DROP_TCP_CSUM           5
#define DROP_SOCKET_FILTER      6
#define DROP_UDP_CSUM           7
#define DROP_NETFILTER_DROP     8
#define DROP_OTHERHOST          9
#define DROP_IP_CSUM            10
#define DROP_IP_INHDR           11
#define DROP_IP_RPFILTER        12
#define DROP_XFRM_POLICY        14
#define DROP_IP_NOPROTO         15
#define DROP_SOCKET_RCVBUFF     16
#define DROP_PROTO_MEM          17
#define DROP_TCP_ZEROWINDOW     28
#define DROP_TCP_OLD_DATA       29
#define DROP_TCP_OVERWINDOW     30
#define DROP_TCP_OFOMERGE       31
#define DROP_TCP_RFC7323_PAWS   32
#define DROP_TCP_INVALID_SEQ    34
#define DROP_TCP_RESET          35
#define DROP_TCP_INVALID_SYN    36
#define DROP_IP_OUTNOROUTES     44
#define DROP_NEIGH_DEAD         50
#define DROP_TC_EGRESS          51
#define DROP_QDISC_DROP         52
#define DROP_CPU_BACKLOG        53
#define DROP_XDP                54
#define DROP_TC_INGRESS         55
#define DROP_UNHANDLED_PROTO    56
#define DROP_SKB_CSUM           57
#define DROP_NOMEM              63
#define DROP_HDR_TRUNC          64

#endif /* __NET_OBSERVER_H */
