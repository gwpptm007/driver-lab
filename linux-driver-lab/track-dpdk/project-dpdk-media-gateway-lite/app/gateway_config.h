/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_config.h - 配置结构与默认值
 *
 * gw_config 包含：
 *   - mbuf pool 参数（nb_mbuf, cache size）
 *   - RX/TX 队列参数（descriptor 数量、burst size）
 *   - 运行参数（run_seconds, stats_period）
 *   - 策略开关（promisc, udp_only, swap_mac, strict_rules）
 *   - 规则表（rules[]）
 */

#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "gateway_common.h"
#include "gateway_rule.h"

/* mbuf 池大小：8192 个 mbuf */
#define GW_DEFAULT_NB_MBUF      8192U
/* per-lcore mbuf cache 大小 */
#define GW_DEFAULT_MBUF_CACHE   250U
/* 默认每次收包 burst 大小 */
#define GW_DEFAULT_BURST_SIZE   32U
/* RX/TX descriptor 数量（网卡队列深度） */
#define GW_DEFAULT_RX_DESC      1024U
#define GW_DEFAULT_TX_DESC      1024U
/* 统计打印周期（秒） */
#define GW_DEFAULT_STATS_PERIOD 2U
/* burst size 上限 */
#define GW_MAX_BURST_SIZE       128U

/*------------------------------------------------------------------------------
 * gw_config - 网关配置结构
 *----------------------------------------------------------------------------*/
struct gw_config {
    uint32_t nb_mbuf;        /* mbuf 池数量 */
    uint32_t mbuf_cache;     /* per-lcore cache 大小 */
    uint16_t burst_size;     /* 每次 RX/TX burst 的包数量 */
    uint16_t rx_desc;        /* RX descriptor 数量 */
    uint16_t tx_desc;        /* TX descriptor 数量 */
    uint32_t run_seconds;    /* 运行时间（秒），0=无限 */
    uint32_t stats_period;   /* 统计打印周期（秒） */
    bool promisc;             /* 混杂模式开关 */
    bool udp_only;           /* UDP-only 策略：只放行 UDP，drop 其他 */
    bool swap_mac;           /* 无 MAC rewrite 时交换 src/dst MAC */
    bool strict_rules;        /* 严格模式：无规则匹配则丢弃 */
    uint16_t nb_rules;       /* 实际规则数量 */
    struct gw_rule rules[GW_MAX_RULES];  /* 规则表 */
};

void gw_config_init(struct gw_config *cfg);
int gw_config_parse_args(struct gw_config *cfg, int argc, char **argv);
void gw_config_print(const struct gw_config *cfg);
void gw_usage(const char *prog, const struct gw_config *cfg);

#endif /* GATEWAY_CONFIG_H */
