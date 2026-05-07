/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_config.c - 命令行参数解析与配置管理
 *
 * 支持两类参数（以 "--" 分隔）：
 *   EAL 参数（rte_eal_init 处理）：-l 0-1 -n 4 --file-prefix xxx 等
 *   APP 参数（gw_config_parse_args 处理）：--run-seconds --rule0 --udp-only 等
 *
 * 规则配置格式：
 *   --rule0 0:1                    → 入端口0 出端口1
 *   --rule0-dst-port 9000          → 匹配 UDP 目的端口 9000
 *   --rule0-rewrite-dst-ip 10.0.0.1 → rewrite 目的 IP
 */

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_ether.h>

#include "gateway_config.h"

/* 解析无符号 32 位整数 */
static int parse_u32(const char *name, const char *value, uint32_t *out)
{
    char *end = NULL;
    unsigned long v;

    if (value == NULL || value[0] == '\0') {
        fprintf(stderr, "missing value for %s\n", name);
        return -1;
    }

    errno = 0;
    v = strtoul(value, &end, 0);
    if (errno != 0 || end == value || *end != '\0' || v > UINT32_MAX) {
        fprintf(stderr, "invalid integer for %s: %s\n", name, value);
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

/* 解析布尔值（0 或 1） */
static int parse_bool_arg(const char *name, const char *value, bool *out)
{
    uint32_t v;
    if (parse_u32(name, value, &v) < 0)
        return -1;
    if (v > 1) {
        fprintf(stderr, "%s must be 0 or 1\n", name);
        return -1;
    }
    *out = (v == 1);
    return 0;
}

/* 解析 MAC 地址格式：xx:xx:xx:xx:xx:xx */
static int parse_mac(const char *text, struct rte_ether_addr *addr)
{
    unsigned int b[6];
    int n;

    n = sscanf(text, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    if (n != 6) {
        fprintf(stderr, "invalid MAC address: %s\n", text);
        return -1;
    }
    for (int i = 0; i < 6; i++) {
        if (b[i] > 0xff) {
            fprintf(stderr, "invalid MAC byte in: %s\n", text);
            return -1;
        }
        addr->addr_bytes[i] = (uint8_t)b[i];
    }
    return 0;
}

/* 解析 IPv4 地址：点分十进制 → 网络字节序 u32 */
static int parse_ipv4(const char *text, uint32_t *addr)
{
    struct in_addr a;
    if (inet_pton(AF_INET, text, &a) != 1) {
        fprintf(stderr, "invalid IPv4 address: %s\n", text);
        return -1;
    }
    *addr = a.s_addr;
    return 0;
}

/* 解析 UDP 端口号：1..65535，转换为网络字节序 */
static int parse_udp_port(const char *name, const char *text, uint16_t *port_be)
{
    uint32_t v;
    if (parse_u32(name, text, &v) < 0)
        return -1;
    if (v == 0 || v > 65535) {
        fprintf(stderr, "%s must be 1..65535\n", name);
        return -1;
    }
    *port_be = rte_cpu_to_be_16((uint16_t)v);
    return 0;
}

/* 解析规则方向：IN_PORT:OUT_PORT 格式 */
static int parse_rule_dir(const char *name, const char *text, struct gw_rule *rule)
{
    unsigned int in_port, out_port;
    if (sscanf(text, "%u:%u", &in_port, &out_port) != 2) {
        fprintf(stderr, "%s expects IN_PORT:OUT_PORT, got: %s\n", name, text);
        return -1;
    }
    if (in_port >= GW_MAX_PORTS || out_port >= GW_MAX_PORTS) {
        fprintf(stderr, "%s port index must be < %u\n", name, GW_MAX_PORTS);
        return -1;
    }
    rule->enabled = true;
    rule->in_port = (uint16_t)in_port;
    rule->out_port = (uint16_t)out_port;
    return 0;
}

/* 更新已见到的最大规则索引（用于 --rule0-dst-port 等子选项） */
static void mark_rule_seen(struct gw_config *cfg, uint16_t idx)
{
    if (idx + 1 > cfg->nb_rules)
        cfg->nb_rules = (uint16_t)(idx + 1);
}

/*------------------------------------------------------------------------------
 * gw_config_init - 初始化配置为默认值
 *----------------------------------------------------------------------------*/
void gw_config_init(struct gw_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->nb_mbuf = GW_DEFAULT_NB_MBUF;
    cfg->mbuf_cache = GW_DEFAULT_MBUF_CACHE;
    cfg->burst_size = GW_DEFAULT_BURST_SIZE;
    cfg->rx_desc = GW_DEFAULT_RX_DESC;
    cfg->tx_desc = GW_DEFAULT_TX_DESC;
    cfg->run_seconds = 20;
    cfg->stats_period = GW_DEFAULT_STATS_PERIOD;
    cfg->promisc = true;     /* 默认开启混杂模式 */
    cfg->udp_only = true;    /* 默认只放行 UDP */
    cfg->swap_mac = true;     /* 默认交换 MAC */
    cfg->strict_rules = true; /* 默认严格模式（无匹配则丢弃） */
    for (uint16_t i = 0; i < GW_MAX_RULES; i++)
        gw_rule_init(&cfg->rules[i], i);
}

/*------------------------------------------------------------------------------
 * gw_usage - 打印帮助信息
 *----------------------------------------------------------------------------*/
void gw_usage(const char *prog, const struct gw_config *cfg)
{
    printf("Usage: %s [EAL options] -- [APP options]\n", prog);
    printf("\nAPP options:\n");
    printf("  --run-seconds N            run duration, 0 means until Ctrl-C (default: %u)\n", cfg->run_seconds);
    printf("  --stats-period N           stats interval seconds (default: %u)\n", cfg->stats_period);
    printf("  --burst-size N             RX/TX burst size, max %u (default: %u)\n", GW_MAX_BURST_SIZE, cfg->burst_size);
    printf("  --nb-mbuf N                mbuf count (default: %u)\n", cfg->nb_mbuf);
    printf("  --rx-desc N                RX descriptors (default: %u)\n", cfg->rx_desc);
    printf("  --tx-desc N                TX descriptors (default: %u)\n", cfg->tx_desc);
    printf("  --promisc 0|1              promiscuous mode (default: %u)\n", cfg->promisc ? 1U : 0U);
    printf("  --udp-only 0|1             drop IPv4 non-UDP, still count ARP (default: %u)\n", cfg->udp_only ? 1U : 0U);
    printf("  --swap-mac 0|1             swap Ethernet src/dst when no MAC rewrite (default: %u)\n", cfg->swap_mac ? 1U : 0U);
    printf("  --strict-rules 0|1         drop packets without route/rule (default: %u)\n", cfg->strict_rules ? 1U : 0U);
    printf("\nRule options: rule id is 0..%u\n", GW_MAX_RULES - 1);
    printf("  --rule0 IN:OUT             enable rule0 direction, e.g. 0:1\n");
    printf("  --rule0-name NAME          optional rule name\n");
    printf("  --rule0-src-ip IPv4        optional match source IPv4\n");
    printf("  --rule0-dst-ip IPv4        optional match destination IPv4\n");
    printf("  --rule0-src-port PORT      optional match UDP source port\n");
    printf("  --rule0-dst-port PORT      optional match UDP destination port\n");
    printf("  --rule0-rewrite-src-mac MAC\n");
    printf("  --rule0-rewrite-dst-mac MAC\n");
    printf("  --rule0-rewrite-src-ip IPv4\n");
    printf("  --rule0-rewrite-dst-ip IPv4\n");
    printf("  --rule0-rewrite-src-port PORT\n");
    printf("  --rule0-rewrite-dst-port PORT\n");
}

/*------------------------------------------------------------------------------
 * parse_rule_option - 解析规则相关选项
 *
 * 支持的规则选项（以 --ruleN- 开头）：
 *   --rule0           → 启用规则 0，指定方向 IN:OUT
 *   --rule0-name      → 规则名称
 *   --rule0-src-ip    → 匹配源 IP
 *   --rule0-dst-ip    → 匹配目的 IP
 *   --rule0-src-port  → 匹配源 UDP 端口
 *   --rule0-dst-port  → 匹配目的 UDP 端口
 *   --rule0-rewrite-xxx → rewrite 动作
 *
 * @return 0=成功, 1=不是规则选项, -1=解析错误
 *----------------------------------------------------------------------------*/
static int parse_rule_option(struct gw_config *cfg, const char *arg, const char *value)
{
    char opt[96];

    for (uint16_t r = 0; r < GW_MAX_RULES; r++) {
        struct gw_rule *rule = &cfg->rules[r];

        snprintf(opt, sizeof(opt), "--rule%u", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            return parse_rule_dir(arg, value, rule);
        }

        snprintf(opt, sizeof(opt), "--rule%u-name", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            snprintf(rule->name, sizeof(rule->name), "%s", value);
            return 0;
        }

        snprintf(opt, sizeof(opt), "--rule%u-src-ip", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->match_src_ip = true;
            return parse_ipv4(value, &rule->src_ip);
        }
        snprintf(opt, sizeof(opt), "--rule%u-dst-ip", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->match_dst_ip = true;
            return parse_ipv4(value, &rule->dst_ip);
        }
        snprintf(opt, sizeof(opt), "--rule%u-src-port", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->match_src_port = true;
            return parse_udp_port(arg, value, &rule->src_port);
        }
        snprintf(opt, sizeof(opt), "--rule%u-dst-port", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->match_dst_port = true;
            return parse_udp_port(arg, value, &rule->dst_port);
        }

        snprintf(opt, sizeof(opt), "--rule%u-rewrite-src-mac", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->set_src_mac = true;
            return parse_mac(value, &rule->rewrite_src_mac);
        }
        snprintf(opt, sizeof(opt), "--rule%u-rewrite-dst-mac", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->set_dst_mac = true;
            return parse_mac(value, &rule->rewrite_dst_mac);
        }
        snprintf(opt, sizeof(opt), "--rule%u-rewrite-src-ip", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->set_src_ip = true;
            return parse_ipv4(value, &rule->rewrite_src_ip);
        }
        snprintf(opt, sizeof(opt), "--rule%u-rewrite-dst-ip", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->set_dst_ip = true;
            return parse_ipv4(value, &rule->rewrite_dst_ip);
        }
        snprintf(opt, sizeof(opt), "--rule%u-rewrite-src-port", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->set_src_port = true;
            return parse_udp_port(arg, value, &rule->rewrite_src_port);
        }
        snprintf(opt, sizeof(opt), "--rule%u-rewrite-dst-port", r);
        if (strcmp(arg, opt) == 0) {
            mark_rule_seen(cfg, r);
            rule->set_dst_port = true;
            return parse_udp_port(arg, value, &rule->rewrite_dst_port);
        }
    }

    return 1; /* not a rule option */
}

/*------------------------------------------------------------------------------
 * gw_config_parse_args - 解析 APP 命令行参数
 *
 * EAL 参数已被 rte_eal_init 消费，剩下的都是 APP 参数
 * 格式：./media-gateway-lite [EAL options] -- [APP options]
 *----------------------------------------------------------------------------*/
int gw_config_parse_args(struct gw_config *cfg, int argc, char **argv)
{
    for (int i = 0; i < argc; i++) {
        const char *arg = argv[i];
        uint32_t v;
        int ret;

        if (arg == NULL)
            continue;

        /* --help / -h：打印帮助后正常退出 */
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            gw_usage("media-gateway-lite", cfg);
            return 1;
        }

        /* 规则选项（--ruleN-xxx 开头）统一处理 */
        if (strncmp(arg, "--rule", 6) == 0) {
            if (++i >= argc) {
                fprintf(stderr, "missing value for %s\n", arg);
                return -1;
            }
            ret = parse_rule_option(cfg, arg, argv[i]);
            if (ret == 0)
                continue;
            if (ret < 0)
                return -1;
            fprintf(stderr, "unknown rule option: %s\n", arg);
            return -1;
        }

        /* 全局选项解析 */
        if (strcmp(arg, "--run-seconds") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &cfg->run_seconds) < 0)
                return -1;
        } else if (strcmp(arg, "--stats-period") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &cfg->stats_period) < 0)
                return -1;
        } else if (strcmp(arg, "--burst-size") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &v) < 0)
                return -1;
            if (v == 0 || v > GW_MAX_BURST_SIZE) {
                fprintf(stderr, "--burst-size must be 1..%u\n", GW_MAX_BURST_SIZE);
                return -1;
            }
            cfg->burst_size = (uint16_t)v;
        } else if (strcmp(arg, "--nb-mbuf") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &cfg->nb_mbuf) < 0)
                return -1;
        } else if (strcmp(arg, "--rx-desc") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &v) < 0)
                return -1;
            cfg->rx_desc = (uint16_t)v;
        } else if (strcmp(arg, "--tx-desc") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &v) < 0)
                return -1;
            cfg->tx_desc = (uint16_t)v;
        } else if (strcmp(arg, "--promisc") == 0) {
            if (++i >= argc || parse_bool_arg(arg, argv[i], &cfg->promisc) < 0)
                return -1;
        } else if (strcmp(arg, "--udp-only") == 0) {
            if (++i >= argc || parse_bool_arg(arg, argv[i], &cfg->udp_only) < 0)
                return -1;
        } else if (strcmp(arg, "--swap-mac") == 0) {
            if (++i >= argc || parse_bool_arg(arg, argv[i], &cfg->swap_mac) < 0)
                return -1;
        } else if (strcmp(arg, "--strict-rules") == 0) {
            if (++i >= argc || parse_bool_arg(arg, argv[i], &cfg->strict_rules) < 0)
                return -1;
        } else if (strncmp(arg, "--", 2) == 0) {
            fprintf(stderr, "unknown APP option: %s\n", arg);
            gw_usage("media-gateway-lite", cfg);
            return -1;
        }
    }

    if (cfg->stats_period == 0)
        cfg->stats_period = GW_DEFAULT_STATS_PERIOD;

    return 0;
}

/*------------------------------------------------------------------------------
 * gw_config_print - 打印当前配置（调试用）
 *----------------------------------------------------------------------------*/
void gw_config_print(const struct gw_config *cfg)
{
    printf("media-gateway-lite config: nb_mbuf=%u cache=%u rx_desc=%u tx_desc=%u burst=%u run_seconds=%u stats_period=%u\n",
           cfg->nb_mbuf, cfg->mbuf_cache, cfg->rx_desc, cfg->tx_desc,
           cfg->burst_size, cfg->run_seconds, cfg->stats_period);
    printf("policy: promisc=%u udp_only=%u swap_mac=%u strict_rules=%u nb_rules=%u\n",
           cfg->promisc ? 1U : 0U,
           cfg->udp_only ? 1U : 0U,
           cfg->swap_mac ? 1U : 0U,
           cfg->strict_rules ? 1U : 0U,
           cfg->nb_rules);
    gw_rules_print(cfg);
}
