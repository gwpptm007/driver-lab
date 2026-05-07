/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_rule.c - 规则表管理
 *
 * 规则结构包含：
 *   - 匹配条件（in_port、src/dst IP、src/dst Port）
 *   - 转发目标（out_port）
 *   - rewrite 动作（MAC、IP、Port 改写）
 *
 * 支持两种查找模式：
 *   - gw_find_udp_rule：五元组精确匹配（in_port + IP + Port）
 *   - gw_find_l2_rule：仅按 in_port 匹配（ARP 或非 UDP 兜底）
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "gateway_config.h"
#include "gateway_rule.h"

/*------------------------------------------------------------------------------
 * gw_rule_init - 初始化单条规则为默认值
 *----------------------------------------------------------------------------*/
void gw_rule_init(struct gw_rule *rule, uint16_t idx)
{
    memset(rule, 0, sizeof(*rule));
    snprintf(rule->name, sizeof(rule->name), "rule%u", idx);
    rule->in_port = UINT16_MAX;
    rule->out_port = UINT16_MAX;
}

/*------------------------------------------------------------------------------
 * gw_rules_prepare_defaults - 自动生成双向默认规则
 *
 * 当用户未通过命令行指定任何规则时调用
 * 自动创建 port0 ↔ port1 的双向 L2 转发规则
 *----------------------------------------------------------------------------*/
void gw_rules_prepare_defaults(struct gw_config *cfg, const uint16_t *ports, uint16_t nb_ports)
{
    if (cfg->nb_rules != 0)
        return;  /* 用户已指定规则，跳过自动生成 */

    if (nb_ports >= 2) {
        /* 规则 0：port0 → port1 */
        gw_rule_init(&cfg->rules[0], 0);
        cfg->rules[0].enabled = true;
        snprintf(cfg->rules[0].name, sizeof(cfg->rules[0].name), "auto_%u_to_%u", ports[0], ports[1]);
        cfg->rules[0].in_port = ports[0];
        cfg->rules[0].out_port = ports[1];

        /* 规则 1：port1 → port0（反向） */
        gw_rule_init(&cfg->rules[1], 1);
        cfg->rules[1].enabled = true;
        snprintf(cfg->rules[1].name, sizeof(cfg->rules[1].name), "auto_%u_to_%u", ports[1], ports[0]);
        cfg->rules[1].in_port = ports[1];
        cfg->rules[1].out_port = ports[0];
        cfg->nb_rules = 2;
        printf("default bidirectional rules installed for ports %u <-> %u\n", ports[0], ports[1]);
    } else {
        printf("single-port mode: no default forwarding rule installed; packets are classified then dropped/no-route\n");
    }
}

/*------------------------------------------------------------------------------
 * rule_udp_match - UDP 规则五元组匹配
 *
 * 可选匹配字段：src_ip、dst_ip、src_port、dst_port
 * 未设置的字段表示不关心（wildcard）
 *----------------------------------------------------------------------------*/
static bool rule_udp_match(const struct gw_rule *rule,
                           const struct rte_ipv4_hdr *ip,
                           const struct rte_udp_hdr *udp)
{
    if (rule->match_src_ip && ip->src_addr != rule->src_ip)
        return false;
    if (rule->match_dst_ip && ip->dst_addr != rule->dst_ip)
        return false;
    if (rule->match_src_port && udp->src_port != rule->src_port)
        return false;
    if (rule->match_dst_port && udp->dst_port != rule->dst_port)
        return false;
    return true;
}

/*------------------------------------------------------------------------------
 * gw_find_udp_rule - 查找匹配的 UDP 规则（五元组精确匹配）
 *
 * 匹配顺序：
 *   1. 规则已启用
 *   2. in_port 匹配
 *   3. 五元组匹配（可选字段为 wildcard）
 *
 * @return 匹配的规则索引，或 -1 表示无匹配
 *----------------------------------------------------------------------------*/
int gw_find_udp_rule(const struct gw_config *cfg, uint16_t in_port,
                     const struct rte_ipv4_hdr *ip, const struct rte_udp_hdr *udp)
{
    for (uint16_t i = 0; i < cfg->nb_rules && i < GW_MAX_RULES; i++) {
        const struct gw_rule *rule = &cfg->rules[i];
        if (!rule->enabled)
            continue;
        if (rule->in_port != in_port)
            continue;
        if (rule_udp_match(rule, ip, udp))
            return i;
    }
    return -1;
}

/*------------------------------------------------------------------------------
 * gw_find_l2_rule - 查找 L2 规则（仅按入端口匹配）
 *
 * 用于 ARP 或非 UDP 报文的兜底转发
 *----------------------------------------------------------------------------*/
int gw_find_l2_rule(const struct gw_config *cfg, uint16_t in_port)
{
    for (uint16_t i = 0; i < cfg->nb_rules && i < GW_MAX_RULES; i++) {
        const struct gw_rule *rule = &cfg->rules[i];
        if (!rule->enabled)
            continue;
        if (rule->in_port == in_port)
            return i;
    }
    return -1;
}

/* 检查规则是否配置了 MAC rewrite */
bool gw_rule_has_mac_rewrite(const struct gw_rule *rule)
{
    return rule->set_src_mac || rule->set_dst_mac;
}

/*------------------------------------------------------------------------------
 * gw_rule_apply_rewrite - 应用规则定义的报文改写
 *
 * 可改写的字段（网络字节序）：
 *   - Ethernet src/dst MAC
 *   - IPv4 src/dst IP
 *   - UDP src/dst Port
 *
 * 注意：改写 IP/Port 后需要重新计算校验和
 *----------------------------------------------------------------------------*/
bool gw_rule_apply_rewrite(const struct gw_rule *rule,
                           struct rte_ether_hdr *eth,
                           struct rte_ipv4_hdr *ip,
                           struct rte_udp_hdr *udp)
{
    bool changed = false;
    bool changed_ip = false;
    bool changed_udp = false;

    if (rule->set_src_mac) {
        eth->src_addr = rule->rewrite_src_mac;
        changed = true;
    }
    if (rule->set_dst_mac) {
        eth->dst_addr = rule->rewrite_dst_mac;
        changed = true;
    }
    if (rule->set_src_ip) {
        ip->src_addr = rule->rewrite_src_ip;
        changed = true;
        changed_ip = true;
    }
    if (rule->set_dst_ip) {
        ip->dst_addr = rule->rewrite_dst_ip;
        changed = true;
        changed_ip = true;
    }
    if (rule->set_src_port) {
        udp->src_port = rule->rewrite_src_port;
        changed = true;
        changed_udp = true;
    }
    if (rule->set_dst_port) {
        udp->dst_port = rule->rewrite_dst_port;
        changed = true;
        changed_udp = true;
    }

    /* IP 头改写后重新计算 IP 校验和 */
    if (changed_ip) {
        ip->hdr_checksum = 0;
        ip->hdr_checksum = rte_ipv4_cksum(ip);
    }
    /* IP 或 UDP 改写后，UDP 校验和需要置零（让网卡或下游重新计算） */
    if (changed_ip || changed_udp)
        udp->dgram_cksum = 0;

    return changed;
}

static void print_ipv4_value(uint32_t be)
{
    char buf[INET_ADDRSTRLEN];
    struct in_addr a;
    a.s_addr = be;
    if (inet_ntop(AF_INET, &a, buf, sizeof(buf)) == NULL)
        snprintf(buf, sizeof(buf), "<invalid>");
    printf("%s", buf);
}

static void print_mac_value(const struct rte_ether_addr *mac)
{
    printf(RTE_ETHER_ADDR_PRT_FMT, RTE_ETHER_ADDR_BYTES(mac));
}

void gw_rules_print(const struct gw_config *cfg)
{
    printf("rules:\n");
    if (cfg->nb_rules == 0) {
        printf("  <none>\n");
        return;
    }

    for (uint16_t i = 0; i < cfg->nb_rules && i < GW_MAX_RULES; i++) {
        const struct gw_rule *r = &cfg->rules[i];
        printf("  rule %u name=%s enabled=%u dir=%u:%u",
               i, r->name, r->enabled ? 1U : 0U, r->in_port, r->out_port);
        if (r->match_src_ip) { printf(" match_src_ip="); print_ipv4_value(r->src_ip); }
        if (r->match_dst_ip) { printf(" match_dst_ip="); print_ipv4_value(r->dst_ip); }
        if (r->match_src_port) printf(" match_src_port=%u", rte_be_to_cpu_16(r->src_port));
        if (r->match_dst_port) printf(" match_dst_port=%u", rte_be_to_cpu_16(r->dst_port));
        if (r->set_src_mac) { printf(" rewrite_src_mac="); print_mac_value(&r->rewrite_src_mac); }
        if (r->set_dst_mac) { printf(" rewrite_dst_mac="); print_mac_value(&r->rewrite_dst_mac); }
        if (r->set_src_ip) { printf(" rewrite_src_ip="); print_ipv4_value(r->rewrite_src_ip); }
        if (r->set_dst_ip) { printf(" rewrite_dst_ip="); print_ipv4_value(r->rewrite_dst_ip); }
        if (r->set_src_port) printf(" rewrite_src_port=%u", rte_be_to_cpu_16(r->rewrite_src_port));
        if (r->set_dst_port) printf(" rewrite_dst_port=%u", rte_be_to_cpu_16(r->rewrite_dst_port));
        printf("\n");
    }
}
