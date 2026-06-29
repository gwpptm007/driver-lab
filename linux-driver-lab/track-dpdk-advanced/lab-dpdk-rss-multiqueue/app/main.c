/* SPDX-License-Identifier: BSD-3-Clause
 *
 * dpdk-rss-queue-probe - Phase 2 DPDK Advanced lab.
 *
 * 这个程序只做 capability probe 和最小 queue 配置尝试：
 *   1. 初始化 EAL 和一个 DPDK port。
 *   2. 查询 rte_eth_dev_info。
 *   3. 打印 max_rx_queues / max_tx_queues / reta_size / RSS offloads。
 *   4. 尝试按 --rx-queues 配置多 RX queue。
 *   5. 输出 PASS_QUEUE_CONFIG 或 BLOCKED_QUEUE_CONFIG。
 *   6. 输出 PASS_RSS_QUERY 或 BLOCKED_RSS。
 *
 * 它不是多核转发器。Phase 2 先回答：
 * "当前 PMD/环境到底支持多少 queue 和 RSS 能力？"
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define DEFAULT_NB_MBUF 8192U
#define DEFAULT_MBUF_CACHE 250U
#define DEFAULT_RX_DESC 512U
#define DEFAULT_TX_DESC 512U
#define DEFAULT_RX_QUEUES 2U
#define DEFAULT_TX_QUEUES 0U
#define MAX_QUEUES 16U

struct app_config {
    uint32_t nb_mbuf;
    uint32_t mbuf_cache;
    uint16_t rx_desc;
    uint16_t tx_desc;
    uint16_t rx_queues;
    uint16_t tx_queues;
    bool enable_rss;
};

static struct app_config cfg = {
    .nb_mbuf = DEFAULT_NB_MBUF,
    .mbuf_cache = DEFAULT_MBUF_CACHE,
    .rx_desc = DEFAULT_RX_DESC,
    .tx_desc = DEFAULT_TX_DESC,
    .rx_queues = DEFAULT_RX_QUEUES,
    .tx_queues = DEFAULT_TX_QUEUES,
    .enable_rss = true,
};

static void usage(const char *prog)
{
    printf("Usage: %s [EAL options] -- [APP options]\n", prog);
    printf("\nAPP options:\n");
    printf("  --rx-queues N    RX queue count to try (default: %u)\n", cfg.rx_queues);
    printf("  --tx-queues N    TX queue count to try (default: %u)\n", cfg.tx_queues);
    printf("  --rx-desc N      RX descriptors per queue (default: %u)\n", cfg.rx_desc);
    printf("  --tx-desc N      TX descriptors per queue (default: %u)\n", cfg.tx_desc);
    printf("  --nb-mbuf N      mbuf count in pool (default: %u)\n", cfg.nb_mbuf);
    printf("  --mbuf-cache N   mempool cache size (default: %u)\n", cfg.mbuf_cache);
    printf("  --enable-rss 0|1 request RSS mq mode when possible (default: %u)\n", cfg.enable_rss ? 1U : 0U);
    printf("  --help           show this help\n");
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

static int parse_bool(const char *name, const char *value, bool *out)
{
    uint32_t v;

    if (parse_u32(name, value, &v) < 0) {
        return -1;
    }
    if (v > 1) {
        fprintf(stderr, "%s must be 0 or 1\n", name);
        return -1;
    }
    *out = (v == 1);
    return 0;
}

static int parse_app_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 1;
        } else if (strcmp(argv[i], "--rx-queues") == 0 && i + 1 < argc) {
            uint32_t v;
            if (parse_u32("--rx-queues", argv[++i], &v) < 0) return -1;
            if (v == 0 || v > MAX_QUEUES) {
                fprintf(stderr, "--rx-queues must be 1..%u\n", MAX_QUEUES);
                return -1;
            }
            cfg.rx_queues = (uint16_t)v;
        } else if (strcmp(argv[i], "--tx-queues") == 0 && i + 1 < argc) {
            uint32_t v;
            if (parse_u32("--tx-queues", argv[++i], &v) < 0) return -1;
            if (v > MAX_QUEUES) {
                fprintf(stderr, "--tx-queues must be 0..%u\n", MAX_QUEUES);
                return -1;
            }
            cfg.tx_queues = (uint16_t)v;
        } else if (strcmp(argv[i], "--rx-desc") == 0 && i + 1 < argc) {
            uint32_t v;
            if (parse_u32("--rx-desc", argv[++i], &v) < 0) return -1;
            cfg.rx_desc = (uint16_t)v;
        } else if (strcmp(argv[i], "--tx-desc") == 0 && i + 1 < argc) {
            uint32_t v;
            if (parse_u32("--tx-desc", argv[++i], &v) < 0) return -1;
            cfg.tx_desc = (uint16_t)v;
        } else if (strcmp(argv[i], "--nb-mbuf") == 0 && i + 1 < argc) {
            if (parse_u32("--nb-mbuf", argv[++i], &cfg.nb_mbuf) < 0) return -1;
        } else if (strcmp(argv[i], "--mbuf-cache") == 0 && i + 1 < argc) {
            if (parse_u32("--mbuf-cache", argv[++i], &cfg.mbuf_cache) < 0) return -1;
        } else if (strcmp(argv[i], "--enable-rss") == 0 && i + 1 < argc) {
            if (parse_bool("--enable-rss", argv[++i], &cfg.enable_rss) < 0) return -1;
        } else {
            fprintf(stderr, "unknown or incomplete option: %s\n", argv[i]);
            usage(argv[0]);
            return -1;
        }
    }

    return 0;
}

static void print_rss_flags(uint64_t flags)
{
    printf("rss_offloads_hex=0x%" PRIx64 "\n", flags);
    printf("rss_offloads_ipv4=%u\n", (flags & RTE_ETH_RSS_IPV4) ? 1U : 0U);
    printf("rss_offloads_tcp=%u\n", (flags & RTE_ETH_RSS_TCP) ? 1U : 0U);
    printf("rss_offloads_udp=%u\n", (flags & RTE_ETH_RSS_UDP) ? 1U : 0U);
}

static void print_queue_lcore_map(void)
{
    unsigned int lcores[MAX_QUEUES];
    unsigned int count = 0;
    unsigned int lcore_id;

    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (count >= MAX_QUEUES) {
            break;
        }
        lcores[count++] = lcore_id;
    }

    if (count == 0) {
        lcores[count++] = rte_lcore_id();
    }

    printf("# QUEUE_TO_CORE_MAP\n");
    for (uint16_t q = 0; q < cfg.rx_queues; q++) {
        printf("queue_map rxq=%u lcore=%u\n", q, lcores[q % count]);
    }
    printf("result_queue_to_core_doc=PASS_QUEUE_TO_CORE_DOC\n");
}

static int configure_queues(uint16_t port_id, const struct rte_eth_dev_info *info)
{
    struct rte_mempool *pool;
    struct rte_eth_conf port_conf;
    int ret;

    if (cfg.rx_queues > info->max_rx_queues) {
        printf("blocked_reason=max_rx_queues_lt_requested requested=%u max_rx_queues=%u\n",
            cfg.rx_queues, info->max_rx_queues);
        printf("result_queue_config=BLOCKED_QUEUE_CONFIG\n");
        return 0;
    }
    if (cfg.tx_queues > info->max_tx_queues) {
        printf("blocked_reason=max_tx_queues_lt_requested requested=%u max_tx_queues=%u\n",
            cfg.tx_queues, info->max_tx_queues);
        printf("result_queue_config=BLOCKED_QUEUE_CONFIG\n");
        return 0;
    }

    memset(&port_conf, 0, sizeof(port_conf));
    if (cfg.enable_rss && info->flow_type_rss_offloads != 0 && cfg.rx_queues > 1) {
        port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
        port_conf.rx_adv_conf.rss_conf.rss_hf = info->flow_type_rss_offloads;
    } else {
        port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
    }

    pool = rte_pktmbuf_pool_create("rss_queue_probe_pool",
        cfg.nb_mbuf, cfg.mbuf_cache, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (pool == NULL) {
        printf("blocked_reason=mempool_create_failed\n");
        printf("result_queue_config=BLOCKED_QUEUE_CONFIG\n");
        return 0;
    }

    ret = rte_eth_dev_configure(port_id, cfg.rx_queues, cfg.tx_queues, &port_conf);
    if (ret < 0) {
        printf("blocked_reason=rte_eth_dev_configure_failed ret=%d\n", ret);
        printf("result_queue_config=BLOCKED_QUEUE_CONFIG\n");
        return 0;
    }

    for (uint16_t q = 0; q < cfg.rx_queues; q++) {
        ret = rte_eth_rx_queue_setup(port_id, q, cfg.rx_desc,
            rte_eth_dev_socket_id(port_id), NULL, pool);
        if (ret < 0) {
            printf("blocked_reason=rx_queue_setup_failed queue=%u ret=%d\n", q, ret);
            printf("result_queue_config=BLOCKED_QUEUE_CONFIG\n");
            return 0;
        }
    }

    for (uint16_t q = 0; q < cfg.tx_queues; q++) {
        ret = rte_eth_tx_queue_setup(port_id, q, cfg.tx_desc,
            rte_eth_dev_socket_id(port_id), NULL);
        if (ret < 0) {
            printf("blocked_reason=tx_queue_setup_failed queue=%u ret=%d\n", q, ret);
            printf("result_queue_config=BLOCKED_QUEUE_CONFIG\n");
            return 0;
        }
    }

    printf("configured_rx_queues=%u configured_tx_queues=%u mq_mode=%u\n",
        cfg.rx_queues, cfg.tx_queues, port_conf.rxmode.mq_mode);
    printf("result_queue_config=PASS_QUEUE_CONFIG\n");
    rte_eth_dev_close(port_id);
    return 0;
}

int main(int argc, char **argv)
{
    struct rte_eth_dev_info info;
    uint16_t nb_ports;
    uint16_t port_id = 0;
    int ret;

    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eal_init failed\n");
    }
    argc -= ret;
    argv += ret;

    ret = parse_app_args(argc, argv);
    if (ret > 0) {
        rte_eal_cleanup();
        return 0;
    }
    if (ret < 0) {
        rte_eal_cleanup();
        return 1;
    }

    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0) {
        rte_eal_cleanup();
        rte_exit(EXIT_FAILURE, "no DPDK ports available\n");
    }

    memset(&info, 0, sizeof(info));
    ret = rte_eth_dev_info_get(port_id, &info);
    if (ret < 0) {
        rte_eal_cleanup();
        rte_exit(EXIT_FAILURE, "rte_eth_dev_info_get failed: %d\n", ret);
    }

    printf("# DPDK_RSS_QUEUE_PROBE\n");
    printf("port_id=%u driver_name=%s nb_ports=%u\n",
        port_id, info.driver_name ? info.driver_name : "<unknown>", nb_ports);
    printf("requested_rx_queues=%u requested_tx_queues=%u enable_rss=%u\n",
        cfg.rx_queues, cfg.tx_queues, cfg.enable_rss ? 1U : 0U);
    printf("max_rx_queues=%u max_tx_queues=%u reta_size=%u hash_key_size=%u\n",
        info.max_rx_queues, info.max_tx_queues, info.reta_size, info.hash_key_size);
    print_rss_flags(info.flow_type_rss_offloads);

    if (info.flow_type_rss_offloads != 0 || info.reta_size != 0) {
        printf("result_rss_query=PASS_RSS_QUERY\n");
    } else {
        printf("blocked_reason=no_rss_offloads_or_reta\n");
        printf("result_rss_query=BLOCKED_RSS\n");
    }

    print_queue_lcore_map();
    configure_queues(port_id, &info);

    rte_eal_cleanup();
    return 0;
}
