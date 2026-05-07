/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_stats.h - 软件统计与硬件统计结构
 *
 * 两套统计体系：
 *   - 软件统计（gw_runtime_stats）：DPDK 应用层计数
 *     per-port + per-rule 维度的 rx/tx/drops/rewrite 等
 *   - 硬件统计（rte_eth_stats）：DPDK ethdev 层统计
 *     由网卡驱动提供，反映网卡硬件层面收发情况
 */

#ifndef GATEWAY_STATS_H
#define GATEWAY_STATS_H

#include <stdint.h>

#include "gateway_common.h"

/*------------------------------------------------------------------------------
 * gw_port_stats - 每端口软件统计计数器
 *
 * 维度：
 *   - 收发计数：rx/rx_bytes、tx/tx_bytes、tx_failed
 *   - 协议分类：arp、ipv4、udp、non_udp
 *   - 丢弃原因：drops、drop_short、drop_non_udp、drop_no_route
 *   - rewrite 计数：rewrite
 *   - per-rule 统计：rule_hit、rule_bytes、rule_rewrite
 *----------------------------------------------------------------------------*/
struct gw_port_stats {
    uint64_t rx;           /* 收到的包数量 */
    uint64_t rx_bytes;     /* 收到的总字节数 */
    uint64_t tx;           /* 发送成功的包数量 */
    uint64_t tx_bytes;     /* 发送的总字节数 */
    uint64_t tx_failed;    /* TX 失败（TX ring 满）次数 */
    uint64_t drops;        /* 总丢弃数 */
    uint64_t arp;          /* ARP 包计数 */
    uint64_t ipv4;         /* IPv4 包计数 */
    uint64_t udp;          /* UDP 包计数 */
    uint64_t non_udp;      /* 非 UDP 包计数 */
    uint64_t rewrite;      /* rewrite 命中总次数 */
    uint64_t drop_short;   /* 报文过短被丢弃 */
    uint64_t drop_non_udp; /* 非 UDP 被 udp_only 策略丢弃 */
    uint64_t drop_no_route;/* 无匹配规则被丢弃 */
    uint64_t rule_hit[GW_MAX_RULES];      /* 每条规则命中次数 */
    uint64_t rule_bytes[GW_MAX_RULES];     /* 每条规则处理的字节数 */
    uint64_t rule_rewrite[GW_MAX_RULES];   /* 每条规则的 rewrite 次数 */
};

/*------------------------------------------------------------------------------
 * gw_runtime_stats - 运行时统计（包含所有端口）
 *----------------------------------------------------------------------------*/
struct gw_runtime_stats {
    struct gw_port_stats port[GW_MAX_PORTS];  /* per-port 统计 */
};

void gw_stats_reset(struct gw_runtime_stats *stats);
void gw_stats_print(const struct gw_runtime_stats *stats, const uint16_t *ports, uint16_t nb_ports, uint16_t nb_rules);
void gw_ethdev_stats_print(const uint16_t *ports, uint16_t nb_ports);

#endif /* GATEWAY_STATS_H */
