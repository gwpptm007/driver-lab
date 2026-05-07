/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_packet.h - 报文分类处理
 *
 * 核心函数 gw_packet_process：
 *   输入：in_port（入端口）、mbuf（报文）、cfg（配置）、stats（统计）
 *   输出：gw_packet_result（分类结果：是否转发、目标端口、命中规则索引）
 *
 * 分类决策：
 *   ARP        → L2 转发
 *   IPv4+UDP   → 五元组规则匹配 + rewrite → 转发
 *   IPv4+非UDP → udp_only 策略决定
 *   其他       → drop
 */

#ifndef GATEWAY_PACKET_H
#define GATEWAY_PACKET_H

#include <stdbool.h>
#include <stdint.h>

#include <rte_mbuf.h>

#include "gateway_config.h"
#include "gateway_stats.h"

/*------------------------------------------------------------------------------
 * gw_packet_result - 报文分类结果
 *----------------------------------------------------------------------------*/
struct gw_packet_result {
    bool forward;       /* 是否转发（true=转发，false=丢弃） */
    uint16_t out_port;  /* 目标端口（forward=true 时有效） */
    int rule_index;     /* 命中的规则索引（用于统计） */
};

/* 报文分类处理入口（见 gateway_packet.c 实现） */
struct gw_packet_result gw_packet_process(uint16_t in_port,
                                          struct rte_mbuf *m,
                                          const struct gw_config *cfg,
                                          struct gw_runtime_stats *stats);

#endif /* GATEWAY_PACKET_H */
