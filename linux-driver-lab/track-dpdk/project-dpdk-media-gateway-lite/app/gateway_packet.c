/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_packet.c - 报文解析与分类处理
 *
 * 协议解析层次：
 *   Ethernet (L2) → IPv4 (L3) → UDP (L4)
 *
 * 分类决策树：
 *   ARP  → L2 转发（只换 MAC）
 *   IPv4 + UDP → 规则匹配 + rewrite → 转发
 *   IPv4 + 非UDP → udp_only 策略决定 drop 或 L2 转发
 *   其他 → drop
 */

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_udp.h>

#include "gateway_packet.h"
#include "gateway_rule.h"

/*------------------------------------------------------------------------------
 * maybe_swap_mac - 交换 Ethernet 源/目的 MAC
 *
 * 当规则没有明确指定 MAC rewrite 时，可选地交换 src/dst MAC
 * 典型用途：L2 转发时，让回复报文能自动路由回去
 *----------------------------------------------------------------------------*/
static void maybe_swap_mac(struct rte_ether_hdr *eth, const struct gw_config *cfg)
{
    struct rte_ether_addr tmp;

    if (!cfg->swap_mac)
        return;
    tmp = eth->src_addr;
    eth->src_addr = eth->dst_addr;
    eth->dst_addr = tmp;
}

/* 构造分类结果：丢弃 */
static struct gw_packet_result drop_result(void)
{
    struct gw_packet_result r = { .forward = false, .out_port = UINT16_MAX, .rule_index = -1 };
    return r;
}

/* 构造分类结果：转发到指定端口 */
static struct gw_packet_result forward_result(uint16_t out_port, int rule_index)
{
    struct gw_packet_result r = { .forward = true, .out_port = out_port, .rule_index = rule_index };
    return r;
}

/*------------------------------------------------------------------------------
 * route_l2 - L2 层转发（ARP 或非 UDP 兜底）
 *
 * 仅根据 in_port 查找规则，不关心 IP/Port 匹配
 * 用于：ARP 解析后转发，或 udp_only=0 时的非 UDP 报文兜底转发
 *----------------------------------------------------------------------------*/
static struct gw_packet_result route_l2(uint16_t in_port,
                                        struct rte_ether_hdr *eth,
                                        const struct gw_config *cfg,
                                        struct gw_runtime_stats *stats)
{
    int ri = gw_find_l2_rule(cfg, in_port);
    if (ri < 0) {
        stats->port[in_port].drop_no_route++;
        return drop_result();
    }
    maybe_swap_mac(eth, cfg);
    return forward_result(cfg->rules[ri].out_port, ri);
}

/*------------------------------------------------------------------------------
 * gw_packet_process - 报文分类处理核心函数
 *
 * 处理流程：
 *   1. 解析 Ethernet header，判断 ether_type
 *   2. ARP → route_l2
 *   3. IPv4 → 继续解析 IP header
 *      - UDP → gw_find_udp_rule 精确匹配 → rewrite → 转发
 *      - 非UDP → udp_only 策略
 *   4. 其他 ether_type → drop 或 L2 兜底
 *
 * @param in_port  收到报文的端口
 * @param m        mbuf 指针（报文数据）
 * @param cfg      网关配置（规则表、策略开关）
 * @param stats    统计计数器指针
 * @return         分类结果（转发目标端口或丢弃）
 *----------------------------------------------------------------------------*/
struct gw_packet_result gw_packet_process(uint16_t in_port,
                                          struct rte_mbuf *m,
                                          const struct gw_config *cfg,
                                          struct gw_runtime_stats *stats)
{
    struct gw_port_stats *ps = &stats->port[in_port];
    struct rte_ether_hdr *eth;
    uint16_t ether_type;

    /* 检查报文长度是否至少有 Ethernet header */
    if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr)) {
        ps->drop_short++;
        return drop_result();
    }

    /* 获取 Ethernet header 指针（mbuf 数据偏移 0） */
    eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    /* 转换字节序：wire byte order → host byte order */
    ether_type = rte_be_to_cpu_16(eth->ether_type);

    /*--------------------- ARP 处理 ---------------------*/
    if (ether_type == RTE_ETHER_TYPE_ARP) {
        ps->arp++;
        return route_l2(in_port, eth, cfg, stats);  /* ARP 只做 L2 转发 */
    }

    /*--------------------- 非 IPv4 处理 ---------------------*/
    if (ether_type != RTE_ETHER_TYPE_IPV4) {
        ps->non_udp++;
        if (cfg->udp_only) {
            /* UDP-only 策略：只允许 UDP 通行，其他全部丢弃 */
            ps->drop_non_udp++;
            return drop_result();
        }
        /* 非 UDP-only 模式：走 L2 兜底转发 */
        return route_l2(in_port, eth, cfg, stats);
    }

    /*--------------------- IPv4 处理 ---------------------*/
    ps->ipv4++;

    /* 检查是否有完整 IPv4 header（IHL * 4 字节） */
    if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr)) {
        ps->drop_short++;
        return drop_result();
    }

    /* 获取 IPv4 header 指针（Ethernet header 之后） */
    struct rte_ipv4_hdr *ip = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
    /* IHL × 4 = IP header 实际长度（可能 > 20 字节如果有 options） */
    const uint8_t ihl = (uint8_t)((ip->version_ihl & 0x0fU) * 4U);
    if (ihl < sizeof(struct rte_ipv4_hdr) ||
        rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr) + ihl) {
        ps->drop_short++;
        return drop_result();
    }

    /*--------------------- 非 UDP 处理 ---------------------*/
    if (ip->next_proto_id != IPPROTO_UDP) {
        ps->non_udp++;
        if (cfg->udp_only) {
            ps->drop_non_udp++;
            return drop_result();
        }
        return route_l2(in_port, eth, cfg, stats);
    }

    /*--------------------- UDP 处理 ---------------------*/
    /* 检查是否有完整 UDP header（8 字节） */
    if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr) + ihl + sizeof(struct rte_udp_hdr)) {
        ps->drop_short++;
        return drop_result();
    }

    ps->udp++;

    /* 获取 UDP header 指针（IP header 之后） */
    struct rte_udp_hdr *udp = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + ihl);

    /* 查找匹配的 UDP 规则（精确五元组匹配） */
    int ri = gw_find_udp_rule(cfg, in_port, ip, udp);
    if (ri < 0) {
        ps->drop_no_route++;
        return drop_result();
    }

    /* 规则命中：应用 rewrite（MAC/IP/Port 改写） */
    const struct gw_rule *rule = &cfg->rules[ri];
    bool changed = gw_rule_apply_rewrite(rule, eth, ip, udp);

    /* 如果规则没有明确设置 MAC rewrite，使用 swap_mac 兜底 */
    if (!gw_rule_has_mac_rewrite(rule))
        maybe_swap_mac(eth, cfg);

    /* 更新规则命中统计 */
    ps->rule_hit[ri]++;
    ps->rule_bytes[ri] += rte_pktmbuf_pkt_len(m);
    if (changed) {
        ps->rewrite++;           /* 全局 rewrite 计数 */
        ps->rule_rewrite[ri]++;  /* 该规则的 rewrite 计数 */
    }

    return forward_result(rule->out_port, ri);
}
