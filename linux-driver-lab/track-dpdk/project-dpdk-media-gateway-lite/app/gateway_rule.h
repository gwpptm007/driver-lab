/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_rule.h - 规则表结构与管理
 *
 * 每条规则包含：
 *   - 匹配条件：in_port + 可选的五元组（src/dst IP、src/dst Port）
 *   - 转发目标：out_port
 *   - rewrite 动作：MAC、IP、Port 的 src/dst 改写
 *
 * 所有 IP/Port 字段使用网络字节序（big-endian）
 */

#ifndef GATEWAY_RULE_H
#define GATEWAY_RULE_H

#include <stdbool.h>
#include <stdint.h>

#include <rte_ether.h>

#include "gateway_common.h"

/*------------------------------------------------------------------------------
 * gw_rule - 单条转发规则
 *
 * 匹配逻辑：所有设置的 match_xxx 字段都必须匹配，才认为规则命中
 * rewrite 逻辑：命中的规则执行所有设置的 set_xxx rewrite 动作
 *----------------------------------------------------------------------------*/
struct gw_rule {
    bool enabled;                    /* 规则是否启用 */
    char name[GW_RULE_NAME_LEN];    /* 规则名称（用于日志） */
    uint16_t in_port;               /* 入端口 */
    uint16_t out_port;              /* 出端口 */

    /* 匹配条件（可选，为 false 表示不关心该字段） */
    bool match_src_ip;              /* 是否匹配源 IP */
    bool match_dst_ip;              /* 是否匹配目的 IP */
    bool match_src_port;            /* 是否匹配源 UDP 端口 */
    bool match_dst_port;            /* 是否匹配目的 UDP 端口 */
    uint32_t src_ip;               /* 匹配的源 IP（网络字节序） */
    uint32_t dst_ip;               /* 匹配的目的 IP（网络字节序） */
    uint16_t src_port;             /* 匹配的源端口（网络字节序） */
    uint16_t dst_port;             /* 匹配的目的端口（网络字节序） */

    /* rewrite 动作（设置为 true 表示需要改写对应字段） */
    bool set_src_mac;
    bool set_dst_mac;
    bool set_src_ip;
    bool set_dst_ip;
    bool set_src_port;
    bool set_dst_port;
    struct rte_ether_addr rewrite_src_mac;  /* rewrite 目标 MAC */
    struct rte_ether_addr rewrite_dst_mac;
    uint32_t rewrite_src_ip;       /* rewrite 目标 IP（网络字节序） */
    uint32_t rewrite_dst_ip;       /* rewrite 目标 IP（网络字节序） */
    uint16_t rewrite_src_port;     /* rewrite 目标端口（网络字节序） */
    uint16_t rewrite_dst_port;     /* rewrite 目标端口（网络字节序） */
};

/* 前向声明（避免循环 include） */
struct gw_config;
struct rte_ipv4_hdr;
struct rte_udp_hdr;
struct rte_ether_hdr;

/* 规则初始化 */
void gw_rule_init(struct gw_rule *rule, uint16_t idx);

/* 自动生成双向默认规则 */
void gw_rules_prepare_defaults(struct gw_config *cfg, const uint16_t *ports, uint16_t nb_ports);

/* 五元组精确匹配（in_port + IP + Port） */
int gw_find_udp_rule(const struct gw_config *cfg, uint16_t in_port,
                     const struct rte_ipv4_hdr *ip, const struct rte_udp_hdr *udp);

/* L2 层匹配（仅按入端口） */
int gw_find_l2_rule(const struct gw_config *cfg, uint16_t in_port);

/* 应用规则定义的 rewrite（改写 MAC/IP/Port 头） */
bool gw_rule_apply_rewrite(const struct gw_rule *rule,
                           struct rte_ether_hdr *eth,
                           struct rte_ipv4_hdr *ip,
                           struct rte_udp_hdr *udp);

/* 检查规则是否配置了 MAC rewrite */
bool gw_rule_has_mac_rewrite(const struct gw_rule *rule);

/* 打印规则表（调试用） */
void gw_rules_print(const struct gw_config *cfg);

#endif /* GATEWAY_RULE_H */
