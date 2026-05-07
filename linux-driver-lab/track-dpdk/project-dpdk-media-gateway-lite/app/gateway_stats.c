/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_stats.c - 软件统计与硬件统计打印
 *
 * 两套统计体系：
 *   - 软件统计（gw_runtime_stats）：DPDK 应用自己计数
 *     包括：rx/tx/drops/arp/ipv4/udp/non_udp/rewrite + per-rule 统计
 *   - 硬件统计（rte_eth_stats）：DPDK ethdev 层统计
 *     包括：ipackets/ibytes/opackets/obytes/imissed/ierrors/oerrors
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <rte_ethdev.h>

#include "gateway_stats.h"

/* 重置所有统计计数器为 0 */
void gw_stats_reset(struct gw_runtime_stats *stats)
{
    memset(stats, 0, sizeof(*stats));
}

/*------------------------------------------------------------------------------
 * gw_stats_print - 打印软件统计（应用层自己维护的计数器）
 *
 * 打印内容：
 *   - per-port：rx/tx/drops/arp/ipv4/udp/non_udp/rewrite/drop原因计数
 *   - per-rule：命中次数/字节数/rewrite次数
 *----------------------------------------------------------------------------*/
void gw_stats_print(const struct gw_runtime_stats *stats, const uint16_t *ports, uint16_t nb_ports, uint16_t nb_rules)
{
    printf("\n==== media-gateway-lite software stats ====\n");
    for (uint16_t i = 0; i < nb_ports; i++) {
        const uint16_t portid = ports[i];
        const struct gw_port_stats *s = &stats->port[portid];

        printf("port %u: rx=%" PRIu64 " rx_bytes=%" PRIu64
               " tx=%" PRIu64 " tx_bytes=%" PRIu64 " tx_failed=%" PRIu64
               " drops=%" PRIu64 " arp=%" PRIu64 " ipv4=%" PRIu64 " udp=%" PRIu64 " non_udp=%" PRIu64
               " rewrite=%" PRIu64 " drop_short=%" PRIu64 " drop_non_udp=%" PRIu64 " drop_no_route=%" PRIu64 "\n",
               portid,
               s->rx, s->rx_bytes,
               s->tx, s->tx_bytes, s->tx_failed,
               s->drops, s->arp, s->ipv4, s->udp, s->non_udp,
               s->rewrite, s->drop_short, s->drop_non_udp, s->drop_no_route);

        /* per-rule 统计：命中次数、字节数、rewrite次数 */
        for (uint16_t r = 0; r < nb_rules && r < GW_MAX_RULES; r++) {
            printf("  rule %u: hit=%" PRIu64 " bytes=%" PRIu64 " rewrite=%" PRIu64 "\n",
                   r, s->rule_hit[r], s->rule_bytes[r], s->rule_rewrite[r]);
        }
    }
    printf("===========================================\n");
}

/*------------------------------------------------------------------------------
 * gw_ethdev_stats_print - 打印 DPDK ethdev 硬件统计
 *
 * ethdev 统计由网卡驱动提供，反映网卡层面的收发情况：
 *   - ipackets/opackets：软件从网卡收到的包数量 / 软件发送给网卡的包数量
 *   - ibytes/obytes：收发字节数
 *   - imissed：因 RX ring 满而丢掉的包（软件来不及取）
 *   - ierrors/oerrors：硬件层面的收发错误
 *----------------------------------------------------------------------------*/
void gw_ethdev_stats_print(const uint16_t *ports, uint16_t nb_ports)
{
    printf("\n==== rte_eth_stats ====\n");
    for (uint16_t i = 0; i < nb_ports; i++) {
        const uint16_t portid = ports[i];
        struct rte_eth_stats st;
        int ret = rte_eth_stats_get(portid, &st);
        if (ret != 0) {
            printf("port %u: rte_eth_stats_get failed ret=%d\n", portid, ret);
            continue;
        }
        printf("port %u: ipackets=%" PRIu64 " ibytes=%" PRIu64
               " opackets=%" PRIu64 " obytes=%" PRIu64
               " imissed=%" PRIu64 " ierrors=%" PRIu64 " oerrors=%" PRIu64 "\n",
               portid, st.ipackets, st.ibytes, st.opackets, st.obytes,
               st.imissed, st.ierrors, st.oerrors);
    }
    printf("=======================\n");
}
