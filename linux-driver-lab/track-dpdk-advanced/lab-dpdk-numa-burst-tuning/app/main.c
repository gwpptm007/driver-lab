/* SPDX-License-Identifier: BSD-3-Clause
 *
 * dpdk-burst-cache-probe - Phase 3 DPDK Advanced lab.
 *
 * 这个程序用于建立调优实验方法，而不是生产压测：
 *   - 参数化 burst size。
 *   - 参数化 mempool cache size。
 *   - 记录 lcore/socket/PMD。
 *   - 从 pcap PMD drain packet，输出 rx_packets、duration、pps。
 *
 * Phase 3 重点是变量控制和记录格式，不把 pcap PMD 结果夸大成真实 NIC pps。
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define DEFAULT_NB_MBUF 16384U
#define DEFAULT_MBUF_CACHE 250U
#define DEFAULT_BURST_SIZE 32U
#define DEFAULT_RX_DESC 1024U
#define DEFAULT_MAX_IDLE_POLLS 100000U
#define MAX_BURST_SIZE 256U

struct app_config {
    uint32_t nb_mbuf;
    uint32_t mbuf_cache;
    uint16_t burst_size;
    uint16_t rx_desc;
    uint32_t max_idle_polls;
};

struct app_result {
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t polls;
    uint64_t empty_polls;
    double duration_sec;
    double pps;
};

static struct app_config cfg = {
    .nb_mbuf = DEFAULT_NB_MBUF,
    .mbuf_cache = DEFAULT_MBUF_CACHE,
    .burst_size = DEFAULT_BURST_SIZE,
    .rx_desc = DEFAULT_RX_DESC,
    .max_idle_polls = DEFAULT_MAX_IDLE_POLLS,
};

static void usage(const char *prog)
{
    printf("Usage: %s [EAL options] -- [APP options]\n", prog);
    printf("  --burst-size N      RX burst size, max %u (default: %u)\n", MAX_BURST_SIZE, cfg.burst_size);
    printf("  --mbuf-cache N      mempool cache size (default: %u)\n", cfg.mbuf_cache);
    printf("  --nb-mbuf N         mbuf count in pool (default: %u)\n", cfg.nb_mbuf);
    printf("  --rx-desc N         RX descriptors per port (default: %u)\n", cfg.rx_desc);
    printf("  --max-idle-polls N  stop after N empty polls once RX has started (default: %u)\n", cfg.max_idle_polls);
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
        } else if (strcmp(argv[i], "--rx-desc") == 0 && i + 1 < argc) {
            uint32_t v;
            if (parse_u32("--rx-desc", argv[++i], &v) < 0) return -1;
            cfg.rx_desc = (uint16_t)v;
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

static int setup_port(uint16_t port_id, struct rte_mempool *pool)
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

static void drain_rx(uint16_t port_id, struct app_result *result)
{
    struct rte_mbuf *pkts[MAX_BURST_SIZE];
    uint64_t start = rte_get_timer_cycles();
    uint64_t end;
    uint64_t hz = rte_get_timer_hz();
    uint32_t idle_after_rx = 0;

    for (;;) {
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, pkts, cfg.burst_size);
        result->polls++;

        if (nb_rx == 0) {
            result->empty_polls++;
            if (result->rx_packets > 0 && ++idle_after_rx >= cfg.max_idle_polls) {
                break;
            }
            continue;
        }

        idle_after_rx = 0;
        for (uint16_t i = 0; i < nb_rx; i++) {
            result->rx_packets++;
            result->rx_bytes += rte_pktmbuf_pkt_len(pkts[i]);
            rte_pktmbuf_free(pkts[i]);
        }
    }

    end = rte_get_timer_cycles();
    result->duration_sec = (double)(end - start) / (double)hz;
    if (result->duration_sec > 0.0) {
        result->pps = (double)result->rx_packets / result->duration_sec;
    }
}

int main(int argc, char **argv)
{
    struct rte_mempool *pool;
    struct rte_eth_dev_info info;
    struct app_result result;
    uint16_t port_id = 0;
    int ret;

    ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "rte_eal_init failed\n");
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

    if (rte_eth_dev_count_avail() == 0) {
        rte_eal_cleanup();
        rte_exit(EXIT_FAILURE, "no DPDK ports available\n");
    }

    memset(&info, 0, sizeof(info));
    rte_eth_dev_info_get(port_id, &info);

    pool = rte_pktmbuf_pool_create("burst_cache_probe_pool",
        cfg.nb_mbuf, cfg.mbuf_cache, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (pool == NULL) {
        rte_eal_cleanup();
        rte_exit(EXIT_FAILURE, "mempool create failed\n");
    }

    ret = setup_port(port_id, pool);
    if (ret < 0) {
        rte_eal_cleanup();
        rte_exit(EXIT_FAILURE, "setup port failed: %d\n", ret);
    }

    memset(&result, 0, sizeof(result));
    drain_rx(port_id, &result);

    printf("RESULT driver=%s lcore=%u socket=%d burst_size=%u mbuf_cache=%u nb_mbuf=%u rx_packets=%" PRIu64
           " rx_bytes=%" PRIu64 " polls=%" PRIu64 " empty_polls=%" PRIu64 " duration_sec=%.6f pps=%.2f\n",
        info.driver_name ? info.driver_name : "unknown",
        rte_lcore_id(),
        rte_socket_id(),
        cfg.burst_size,
        cfg.mbuf_cache,
        cfg.nb_mbuf,
        result.rx_packets,
        result.rx_bytes,
        result.polls,
        result.empty_polls,
        result.duration_sec,
        result.pps);

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    rte_eal_cleanup();
    return result.rx_packets > 0 ? 0 : 2;
}
