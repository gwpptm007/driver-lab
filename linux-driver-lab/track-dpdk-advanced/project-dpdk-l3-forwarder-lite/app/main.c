/* SPDX-License-Identifier: BSD-3-Clause
 *
 * dpdk-l3-forwarder-lite - Phase 5 DPDK Advanced project.
 *
 * 这个程序刻意保持“小而完整”：
 *   - port0 使用 pcap PMD 读入测试流量；
 *   - port1 使用 net_null PMD 作为 TX sink；
 *   - 解析 Ethernet / IPv4 / UDP；
 *   - 先执行 ACL drop，再执行简化 route lookup；
 *   - 输出 route/ACL/per-port 统计，方便脚本做验收。
 *
 * 它不是生产 l3fwd 的替代品，而是把 L3 fastpath 的关键骨架拆开讲清楚。
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_udp.h>

#define DEFAULT_NB_MBUF 8192U
#define DEFAULT_MBUF_CACHE 250U
#define DEFAULT_BURST_SIZE 32U
#define DEFAULT_RX_DESC 1024U
#define DEFAULT_TX_DESC 1024U
#define DEFAULT_MAX_IDLE_POLLS 100000U
#define MAX_BURST_SIZE 256U
#define MAX_ROUTES 8U
#define MAX_ACL_RULES 8U

struct route_rule {
    uint32_t prefix_be;
    uint32_t mask_be;
    uint8_t prefix_len;
    uint16_t out_port;
    uint64_t hits;
    uint64_t bytes;
};

struct acl_rule {
    uint16_t udp_dst_port;
    uint64_t drops;
    uint64_t bytes;
};

struct app_config {
    uint32_t nb_mbuf;
    uint32_t mbuf_cache;
    uint16_t burst_size;
    uint16_t rx_desc;
    uint16_t tx_desc;
    uint32_t max_idle_polls;
    uint16_t in_port;
    uint16_t out_port;
    uint32_t nb_routes;
    uint32_t nb_acl_rules;
    struct route_rule routes[MAX_ROUTES];
    struct acl_rule acl[MAX_ACL_RULES];
};

struct app_stats {
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t forwarded_packets;
    uint64_t forwarded_bytes;
    uint64_t tx_failed;
    uint64_t acl_drops;
    uint64_t route_miss_drops;
    uint64_t non_ipv4_drops;
    uint64_t parse_drops;
    uint64_t polls;
    uint64_t empty_polls;
};

static struct app_config cfg = {
    .nb_mbuf = DEFAULT_NB_MBUF,
    .mbuf_cache = DEFAULT_MBUF_CACHE,
    .burst_size = DEFAULT_BURST_SIZE,
    .rx_desc = DEFAULT_RX_DESC,
    .tx_desc = DEFAULT_TX_DESC,
    .max_idle_polls = DEFAULT_MAX_IDLE_POLLS,
    .in_port = 0,
    .out_port = 1,
};

static struct app_stats stats;

static uint32_t ipv4_be(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return RTE_BE32(((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d);
}

static void load_default_rules(void)
{
    /* ACL 优先级高于 route：命中 UDP dst port 9999 的包直接 drop。 */
    cfg.nb_acl_rules = 1;
    cfg.acl[0].udp_dst_port = 9999;

    /* 简化 route table：10.20.0.0/24 从 port1 转发。 */
    cfg.nb_routes = 1;
    cfg.routes[0].prefix_be = ipv4_be(10, 20, 0, 0);
    cfg.routes[0].mask_be = ipv4_be(255, 255, 255, 0);
    cfg.routes[0].prefix_len = 24;
    cfg.routes[0].out_port = cfg.out_port;
}

static void usage(const char *prog)
{
    printf("Usage: %s [EAL options] -- [APP options]\n", prog);
    printf("  --burst-size N       RX/TX burst size, max %u (default: %u)\n", MAX_BURST_SIZE, cfg.burst_size);
    printf("  --mbuf-cache N       mempool cache size (default: %u)\n", cfg.mbuf_cache);
    printf("  --nb-mbuf N          mbuf count in pool (default: %u)\n", cfg.nb_mbuf);
    printf("  --max-idle-polls N   stop after N empty polls once RX has started (default: %u)\n", cfg.max_idle_polls);
}

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

static int parse_app_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 1;
        } else if (strcmp(argv[i], "--burst-size") == 0 && i + 1 < argc) {
            uint32_t v;
            if (parse_u32("--burst-size", argv[++i], &v) < 0) return -1;
            if (v == 0 || v > MAX_BURST_SIZE) return -1;
            cfg.burst_size = (uint16_t)v;
        } else if (strcmp(argv[i], "--mbuf-cache") == 0 && i + 1 < argc) {
            if (parse_u32("--mbuf-cache", argv[++i], &cfg.mbuf_cache) < 0) return -1;
        } else if (strcmp(argv[i], "--nb-mbuf") == 0 && i + 1 < argc) {
            if (parse_u32("--nb-mbuf", argv[++i], &cfg.nb_mbuf) < 0) return -1;
        } else if (strcmp(argv[i], "--max-idle-polls") == 0 && i + 1 < argc) {
            if (parse_u32("--max-idle-polls", argv[++i], &cfg.max_idle_polls) < 0) return -1;
        } else {
            fprintf(stderr, "unknown or incomplete option: %s\n", argv[i]);
            usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

static int setup_rx_port(uint16_t port_id, struct rte_mempool *pool)
{
    struct rte_eth_conf port_conf;
    int ret;

    memset(&port_conf, 0, sizeof(port_conf));
    port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;

    ret = rte_eth_dev_configure(port_id, 1, 0, &port_conf);
    if (ret < 0) return ret;

    ret = rte_eth_rx_queue_setup(port_id, 0, cfg.rx_desc,
        rte_eth_dev_socket_id(port_id), NULL, pool);
    if (ret < 0) return ret;

    return rte_eth_dev_start(port_id);
}

static int setup_tx_port(uint16_t port_id)
{
    struct rte_eth_conf port_conf;
    int ret;

    memset(&port_conf, 0, sizeof(port_conf));

    ret = rte_eth_dev_configure(port_id, 0, 1, &port_conf);
    if (ret < 0) return ret;

    ret = rte_eth_tx_queue_setup(port_id, 0, cfg.tx_desc,
        rte_eth_dev_socket_id(port_id), NULL);
    if (ret < 0) return ret;

    return rte_eth_dev_start(port_id);
}

static bool acl_drop(const struct rte_udp_hdr *udp, uint32_t pkt_len)
{
    const uint16_t dst_port = rte_be_to_cpu_16(udp->dst_port);

    for (uint32_t i = 0; i < cfg.nb_acl_rules; i++) {
        if (dst_port == cfg.acl[i].udp_dst_port) {
            cfg.acl[i].drops++;
            cfg.acl[i].bytes += pkt_len;
            stats.acl_drops++;
            return true;
        }
    }
    return false;
}

static struct route_rule *route_lookup(uint32_t dst_addr_be)
{
    struct route_rule *best = NULL;

    for (uint32_t i = 0; i < cfg.nb_routes; i++) {
        if ((dst_addr_be & cfg.routes[i].mask_be) != cfg.routes[i].prefix_be)
            continue;
        if (best == NULL || cfg.routes[i].prefix_len > best->prefix_len)
            best = &cfg.routes[i];
    }
    return best;
}

static void process_packet(struct rte_mbuf *m)
{
    struct rte_ether_hdr *eth;
    struct rte_ipv4_hdr *ip;
    struct rte_udp_hdr *udp;
    struct route_rule *route;
    const uint32_t pkt_len = rte_pktmbuf_pkt_len(m);
    uint16_t ip_hlen;

    stats.rx_packets++;
    stats.rx_bytes += pkt_len;

    eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    if (unlikely(pkt_len < sizeof(*eth) + sizeof(*ip))) {
        stats.parse_drops++;
        rte_pktmbuf_free(m);
        return;
    }

    if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
        stats.non_ipv4_drops++;
        rte_pktmbuf_free(m);
        return;
    }

    ip = (struct rte_ipv4_hdr *)(eth + 1);
    ip_hlen = (uint16_t)((ip->version_ihl & 0x0f) * 4);
    if (unlikely(ip_hlen < sizeof(*ip) || pkt_len < sizeof(*eth) + ip_hlen + sizeof(*udp))) {
        stats.parse_drops++;
        rte_pktmbuf_free(m);
        return;
    }

    if (ip->next_proto_id != IPPROTO_UDP) {
        stats.parse_drops++;
        rte_pktmbuf_free(m);
        return;
    }

    udp = (struct rte_udp_hdr *)((char *)ip + ip_hlen);

    if (acl_drop(udp, pkt_len)) {
        rte_pktmbuf_free(m);
        return;
    }

    route = route_lookup(ip->dst_addr);
    if (route == NULL) {
        stats.route_miss_drops++;
        rte_pktmbuf_free(m);
        return;
    }

    route->hits++;
    route->bytes += pkt_len;

    struct rte_mbuf *tx_pkts[1] = { m };
    if (rte_eth_tx_burst(route->out_port, 0, tx_pkts, 1) == 1) {
        stats.forwarded_packets++;
        stats.forwarded_bytes += pkt_len;
    } else {
        stats.tx_failed++;
        rte_pktmbuf_free(m);
    }
}

static void run_loop(void)
{
    struct rte_mbuf *pkts[MAX_BURST_SIZE];
    uint32_t idle_after_rx = 0;

    for (;;) {
        uint16_t nb_rx = rte_eth_rx_burst(cfg.in_port, 0, pkts, cfg.burst_size);
        stats.polls++;

        if (nb_rx == 0) {
            stats.empty_polls++;
            if (stats.rx_packets > 0 && ++idle_after_rx >= cfg.max_idle_polls)
                break;
            continue;
        }

        idle_after_rx = 0;
        for (uint16_t i = 0; i < nb_rx; i++)
            process_packet(pkts[i]);
    }
}

static void print_config(void)
{
    printf("CONFIG in_port=%u out_port=%u burst=%u nb_mbuf=%u mbuf_cache=%u\n",
        cfg.in_port, cfg.out_port, cfg.burst_size, cfg.nb_mbuf, cfg.mbuf_cache);
    for (uint32_t i = 0; i < cfg.nb_routes; i++) {
        printf("ROUTE[%u] prefix=10.20.0.0/%u out_port=%u\n",
            i, cfg.routes[i].prefix_len, cfg.routes[i].out_port);
    }
    for (uint32_t i = 0; i < cfg.nb_acl_rules; i++)
        printf("ACL[%u] action=drop udp_dst_port=%u\n", i, cfg.acl[i].udp_dst_port);
}

static void print_stats(void)
{
    printf("RESULT rx_packets=%" PRIu64 " rx_bytes=%" PRIu64
           " forwarded_packets=%" PRIu64 " forwarded_bytes=%" PRIu64
           " acl_drops=%" PRIu64 " route_miss_drops=%" PRIu64
           " non_ipv4_drops=%" PRIu64 " parse_drops=%" PRIu64
           " tx_failed=%" PRIu64 " polls=%" PRIu64 " empty_polls=%" PRIu64 "\n",
           stats.rx_packets, stats.rx_bytes,
           stats.forwarded_packets, stats.forwarded_bytes,
           stats.acl_drops, stats.route_miss_drops,
           stats.non_ipv4_drops, stats.parse_drops,
           stats.tx_failed, stats.polls, stats.empty_polls);

    for (uint32_t i = 0; i < cfg.nb_routes; i++) {
        printf("ROUTE_STATS[%u] hits=%" PRIu64 " bytes=%" PRIu64 "\n",
            i, cfg.routes[i].hits, cfg.routes[i].bytes);
    }
    for (uint32_t i = 0; i < cfg.nb_acl_rules; i++) {
        printf("ACL_STATS[%u] drops=%" PRIu64 " bytes=%" PRIu64 "\n",
            i, cfg.acl[i].drops, cfg.acl[i].bytes);
    }
}

int main(int argc, char **argv)
{
    struct rte_mempool *pool;
    uint16_t nb_ports;
    int ret;

    load_default_rules();

    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        fprintf(stderr, "rte_eal_init failed\n");
        return 1;
    }
    argc -= ret;
    argv += ret;

    ret = parse_app_args(argc, argv);
    if (ret > 0) return 0;
    if (ret < 0) return 1;

    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < 2) {
        fprintf(stderr, "need at least 2 DPDK ports, got %u\n", nb_ports);
        return 1;
    }

    pool = rte_pktmbuf_pool_create("l3_forwarder_pool",
        cfg.nb_mbuf, cfg.mbuf_cache, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (pool == NULL) {
        fprintf(stderr, "rte_pktmbuf_pool_create failed\n");
        return 1;
    }

    print_config();

    if (setup_rx_port(cfg.in_port, pool) < 0) {
        fprintf(stderr, "setup RX port %u failed\n", cfg.in_port);
        return 1;
    }
    if (setup_tx_port(cfg.out_port) < 0) {
        fprintf(stderr, "setup TX port %u failed\n", cfg.out_port);
        rte_eth_dev_stop(cfg.in_port);
        return 1;
    }

    printf("enter l3 forwarder loop on lcore %u\n", rte_lcore_id());
    run_loop();
    print_stats();

    rte_eth_dev_stop(cfg.in_port);
    rte_eth_dev_stop(cfg.out_port);
    rte_eal_cleanup();
    return 0;
}

