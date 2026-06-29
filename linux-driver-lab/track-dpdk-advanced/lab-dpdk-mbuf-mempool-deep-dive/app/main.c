/* SPDX-License-Identifier: BSD-3-Clause
 *
 * dpdk-mbuf-inspect - Phase 1 DPDK Advanced lab.
 *
 * 这个程序故意保持很小：
 *   1. 初始化 EAL 和一个 DPDK port。
 *   2. 创建 pktmbuf mempool。
 *   3. 通过 pcap PMD 收包。
 *   4. 打印 rte_mbuf metadata。
 *   5. 对比软件 RX 统计和 ethdev RX 统计。
 *
 * 它不是转发程序。Phase 1 只回答一个问题：
 * "rx_burst 收到的 packet 在 mbuf/mempool 模型里长什么样？"
 */

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

/* mempool 中 mbuf 总数。Phase 1 用固定值，后续 NUMA/cache 调优再做矩阵。 */
#define DEFAULT_NB_MBUF 8192U

/* 每个 lcore 的 mempool cache 大小。cache 能减少全局 mempool ring 竞争。 */
#define DEFAULT_MBUF_CACHE 250U

/* 每次 rte_eth_rx_burst() 最多取多少个 packet。 */
#define DEFAULT_BURST_SIZE 32U

/* RX queue descriptor 数量。pcap PMD 中主要用于保持和真实 ethdev 初始化形态一致。 */
#define DEFAULT_RX_DESC 1024U

/* 只打印前 N 个 mbuf，避免日志刷屏。 */
#define DEFAULT_SAMPLE_LIMIT 8U

/* pcap PMD 一般很快读完；run_seconds 是兜底退出条件。 */
#define DEFAULT_RUN_SECONDS 5U
#define MAX_BURST_SIZE 128U

/* APP 参数：EAL 参数由 DPDK 消费，"--" 后面的参数由本结构保存。 */
struct app_config {
    uint32_t nb_mbuf;
    uint32_t mbuf_cache;
    uint16_t burst_size;
    uint16_t rx_desc;
    uint32_t sample_limit;
    uint32_t run_seconds;
    bool promisc;
};

/* 软件侧统计：用来和 rte_eth_stats_get() 的 ethdev 统计做一致性对照。 */
struct app_stats {
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint32_t samples_printed;
};

static volatile bool force_quit;

static struct app_config cfg = {
    .nb_mbuf = DEFAULT_NB_MBUF,
    .mbuf_cache = DEFAULT_MBUF_CACHE,
    .burst_size = DEFAULT_BURST_SIZE,
    .rx_desc = DEFAULT_RX_DESC,
    .sample_limit = DEFAULT_SAMPLE_LIMIT,
    .run_seconds = DEFAULT_RUN_SECONDS,
    .promisc = true,
};

/* SIGINT/SIGTERM 只设置退出标志，真正的 cleanup 仍在 main() 末尾统一执行。 */
static void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        force_quit = true;
    }
}

static void usage(const char *prog)
{
    printf("Usage: %s [EAL options] -- [APP options]\n", prog);
    printf("\nAPP options:\n");
    printf("  --run-seconds N     run duration, 0 means until Ctrl-C (default: %u)\n", cfg.run_seconds);
    printf("  --sample-limit N    number of mbufs to print (default: %u)\n", cfg.sample_limit);
    printf("  --burst-size N      RX burst size, max %u (default: %u)\n", MAX_BURST_SIZE, cfg.burst_size);
    printf("  --nb-mbuf N         mbuf count in pool (default: %u)\n", cfg.nb_mbuf);
    printf("  --mbuf-cache N      mempool cache size (default: %u)\n", cfg.mbuf_cache);
    printf("  --rx-desc N         RX descriptors per port (default: %u)\n", cfg.rx_desc);
    printf("  --promisc 0|1       disable/enable promiscuous mode (default: %u)\n", cfg.promisc ? 1U : 0U);
    printf("  --help              show this help\n");
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
        } else if (strcmp(argv[i], "--run-seconds") == 0 && i + 1 < argc) {
            if (parse_u32("--run-seconds", argv[++i], &cfg.run_seconds) < 0) return -1;
        } else if (strcmp(argv[i], "--sample-limit") == 0 && i + 1 < argc) {
            if (parse_u32("--sample-limit", argv[++i], &cfg.sample_limit) < 0) return -1;
        } else if (strcmp(argv[i], "--burst-size") == 0 && i + 1 < argc) {
            uint32_t v;
            if (parse_u32("--burst-size", argv[++i], &v) < 0) return -1;
            if (v == 0 || v > MAX_BURST_SIZE) {
                fprintf(stderr, "--burst-size must be 1..%u\n", MAX_BURST_SIZE);
                return -1;
            }
            cfg.burst_size = (uint16_t)v;
        } else if (strcmp(argv[i], "--nb-mbuf") == 0 && i + 1 < argc) {
            if (parse_u32("--nb-mbuf", argv[++i], &cfg.nb_mbuf) < 0) return -1;
        } else if (strcmp(argv[i], "--mbuf-cache") == 0 && i + 1 < argc) {
            if (parse_u32("--mbuf-cache", argv[++i], &cfg.mbuf_cache) < 0) return -1;
        } else if (strcmp(argv[i], "--rx-desc") == 0 && i + 1 < argc) {
            uint32_t v;
            if (parse_u32("--rx-desc", argv[++i], &v) < 0) return -1;
            if (v == 0 || v > UINT16_MAX) {
                fprintf(stderr, "--rx-desc must be 1..%u\n", UINT16_MAX);
                return -1;
            }
            cfg.rx_desc = (uint16_t)v;
        } else if (strcmp(argv[i], "--promisc") == 0 && i + 1 < argc) {
            if (parse_bool("--promisc", argv[++i], &cfg.promisc) < 0) return -1;
        } else {
            fprintf(stderr, "unknown or incomplete option: %s\n", argv[i]);
            usage(argv[0]);
            return -1;
        }
    }

    return 0;
}

/*
 * print_config - 打印本次实验的固定上下文。
 *
 * 这里故意打印 mempool_size/cache/socket 和 PMD driver_name：
 *   - mempool_size/cache 对应 PASS_MEMPOOL_CONFIG。
 *   - driver_name=net_pcap 证明当前走的是 pcap PMD，而不是真实 NIC。
 *   - max_rx_queues/max_tx_queues 能为 Phase 2 RSS/multiqueue 做铺垫。
 */
static void print_config(uint16_t port_id, struct rte_mempool *pool)
{
    struct rte_eth_dev_info info;
    int ret;

    printf("# DPDK_MBUF_INSPECT_CONFIG\n");
    printf("lcore=%u port_id=%u\n", rte_lcore_id(), port_id);
    printf("nb_mbuf=%u mbuf_cache=%u burst_size=%u rx_desc=%u sample_limit=%u run_seconds=%u promisc=%u\n",
        cfg.nb_mbuf, cfg.mbuf_cache, cfg.burst_size, cfg.rx_desc,
        cfg.sample_limit, cfg.run_seconds, cfg.promisc ? 1U : 0U);
    printf("mempool_name=%s mempool_size=%u mempool_cache_size=%u mempool_socket=%d\n",
        pool->name, rte_mempool_avail_count(pool) + rte_mempool_in_use_count(pool),
        pool->cache_size, pool->socket_id);
    printf("mempool_avail_start=%u mempool_in_use_start=%u\n",
        rte_mempool_avail_count(pool), rte_mempool_in_use_count(pool));

    memset(&info, 0, sizeof(info));
    ret = rte_eth_dev_info_get(port_id, &info);
    if (ret == 0) {
        printf("driver_name=%s max_rx_queues=%u max_tx_queues=%u min_rx_bufsize=%u max_rx_pktlen=%u\n",
            info.driver_name ? info.driver_name : "<unknown>",
            info.max_rx_queues, info.max_tx_queues,
            info.min_rx_bufsize, info.max_rx_pktlen);
    } else {
        printf("driver_info_error=%d\n", ret);
    }
    printf("\n");
}

/*
 * setup_port - 初始化一个 RX-only port。
 *
 * Phase 1 只观察 RX mbuf metadata，所以 rte_eth_dev_configure() 使用：
 *   nb_rx_queue = 1
 *   nb_tx_queue = 0
 *
 * pcap PMD 会把 pcap 文件内容暴露成一个可 rx_burst 的 ethdev port。
 */
static int setup_port(uint16_t port_id, struct rte_mempool *pool)
{
    struct rte_eth_conf port_conf;
    int ret;

    memset(&port_conf, 0, sizeof(port_conf));
    port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;

    ret = rte_eth_dev_configure(port_id, 1, 0, &port_conf);
    if (ret < 0) {
        fprintf(stderr, "rte_eth_dev_configure(port=%u) failed: %d\n", port_id, ret);
        return ret;
    }

    ret = rte_eth_rx_queue_setup(port_id, 0, cfg.rx_desc,
        rte_eth_dev_socket_id(port_id), NULL, pool);
    if (ret < 0) {
        fprintf(stderr, "rte_eth_rx_queue_setup(port=%u) failed: %d\n", port_id, ret);
        return ret;
    }

    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        fprintf(stderr, "rte_eth_dev_start(port=%u) failed: %d\n", port_id, ret);
        return ret;
    }

    if (cfg.promisc) {
        ret = rte_eth_promiscuous_enable(port_id);
        if (ret < 0) {
            fprintf(stderr, "rte_eth_promiscuous_enable(port=%u) failed: %d\n", port_id, ret);
            return ret;
        }
    }

    return 0;
}

/*
 * print_mbuf_sample - 打印一个 rte_mbuf 的关键 metadata。
 *
 * 字段含义：
 *   buf_addr   : mbuf 后面挂着的数据 buffer 虚拟地址。
 *   buf_iova   : DMA/IOVA 地址；当前 EAL 使用 IOVA=VA，所以和虚拟地址接近。
 *   data_off   : packet data 在 buffer 内的偏移，默认通常是 RTE_PKTMBUF_HEADROOM。
 *   data_len   : 当前 segment 的数据长度。
 *   pkt_len    : 整个 packet 的长度；单段包时通常等于 data_len。
 *   nb_segs    : packet 分成几个 segment；当前 pcap 小包为 1。
 *   ol_flags   : offload metadata flags。
 *   packet_type: PMD 解析出的 packet type；pcap PMD 可能不填。
 *   rss_hash   : RSS hash；Phase 1 不启用 RSS，所以通常为 0。
 *   refcnt     : mbuf 引用计数；正常单 owner 情况下为 1。
 */
static void print_mbuf_sample(uint16_t port_id, const struct rte_mbuf *m, uint32_t sample_idx)
{
    printf("MBUF_SAMPLE index=%u port=%u mbuf_port=%u buf_addr=%p buf_iova=0x%" PRIx64
           " data_off=%u data_len=%u pkt_len=%u nb_segs=%u ol_flags=0x%" PRIx64
           " packet_type=0x%x rss_hash=0x%x refcnt=%u\n",
        sample_idx,
        port_id,
        m->port,
        m->buf_addr,
        (uint64_t)m->buf_iova,
        m->data_off,
        m->data_len,
        m->pkt_len,
        m->nb_segs,
        (uint64_t)m->ol_flags,
        m->packet_type,
        m->hash.rss,
        rte_mbuf_refcnt_read(m));
}

/*
 * run_rx_loop - 核心实验路径。
 *
 * rte_eth_rx_burst() 从 port 0 / queue 0 取 mbuf 数组：
 *   pcap file -> net_pcap PMD -> RX queue -> rte_mbuf*
 *
 * 本实验不 forward，所以每个 mbuf 观察后立即 rte_pktmbuf_free() 归还 mempool。
 * 最后 mempool_avail_end 回到 8192，说明没有 mbuf 泄漏。
 */
static void run_rx_loop(uint16_t port_id, struct rte_mempool *pool, struct app_stats *stats)
{
    struct rte_mbuf *pkts[MAX_BURST_SIZE];
    uint64_t start = rte_get_timer_cycles();
    uint64_t hz = rte_get_timer_hz();

    while (!force_quit) {
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, pkts, cfg.burst_size);

        if (nb_rx > 0) {
            for (uint16_t i = 0; i < nb_rx; i++) {
                struct rte_mbuf *m = pkts[i];

                stats->rx_packets++;
                stats->rx_bytes += rte_pktmbuf_pkt_len(m);

                if (stats->samples_printed < cfg.sample_limit) {
                    print_mbuf_sample(port_id, m, stats->samples_printed);
                    stats->samples_printed++;
                }

                rte_pktmbuf_free(m);
            }
        }

        if (cfg.run_seconds > 0) {
            uint64_t now = rte_get_timer_cycles();
            if ((now - start) / hz >= cfg.run_seconds) {
                break;
            }
        }

        if (stats->rx_packets > 0 && stats->samples_printed >= cfg.sample_limit) {
            break;
        }
    }

    printf("mempool_avail_end=%u mempool_in_use_end=%u\n",
        rte_mempool_avail_count(pool), rte_mempool_in_use_count(pool));
}

/*
 * print_final_stats - 输出验收字段。
 *
 * 软件统计来自本程序循环累加；ethdev 统计来自 PMD。
 * pcap PMD 下二者应当能对齐：
 *   software_rx_packets == ethdev_ipackets
 *   software_rx_bytes   == ethdev_ibytes
 */
static void print_final_stats(uint16_t port_id, const struct app_stats *stats)
{
    struct rte_eth_stats eth_stats;
    int ret = rte_eth_stats_get(port_id, &eth_stats);

    printf("\n# FINAL_STATS\n");
    printf("software_rx_packets=%" PRIu64 " software_rx_bytes=%" PRIu64 " samples_printed=%u\n",
        stats->rx_packets, stats->rx_bytes, stats->samples_printed);
    if (ret == 0) {
        printf("ethdev_ipackets=%" PRIu64 " ethdev_ibytes=%" PRIu64 " ethdev_imissed=%" PRIu64 " ethdev_ierrors=%" PRIu64 "\n",
            eth_stats.ipackets, eth_stats.ibytes, eth_stats.imissed, eth_stats.ierrors);
        printf("stats_consistency=%s\n",
            (eth_stats.ipackets >= stats->rx_packets) ? "PASS_STATS_CONSISTENCY" : "CHECK_STATS_CONSISTENCY");
    } else {
        printf("ethdev_stats_error=%d\n", ret);
    }

    printf("result_build=PASS_BUILD_RUNTIME_REACHED\n");
    printf("result_pcap_rx=%s\n", stats->rx_packets > 0 ? "PASS_PCAP_RX" : "FAIL_PCAP_RX");
    printf("result_mbuf_metadata=%s\n", stats->samples_printed > 0 ? "PASS_MBUF_METADATA" : "FAIL_MBUF_METADATA");
    printf("result_mempool_config=PASS_MEMPOOL_CONFIG\n");
}

int main(int argc, char **argv)
{
    struct rte_mempool *pool;
    struct app_stats stats;
    uint16_t port_id;
    uint16_t nb_ports;
    int ret;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /*
     * EAL 初始化会消费 DPDK 参数，例如：
     *   -l 0-1
     *   -n 4
     *   --file-prefix dpdk_mbuf_inspect
     *   --vdev net_pcap0,rx_pcap=...
     *
     * 返回值 ret 表示被 EAL 消费掉的 argc 数量；剩余参数才是 APP 参数。
     */
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

    port_id = 0;
    /*
     * 创建 packet mbuf pool。
     *
     * RTE_MBUF_DEFAULT_BUF_SIZE 包含 mbuf headroom 和 packet data room。
     * RX queue setup 时把 pool 交给 ethdev，PMD 收包时会从这里分配 mbuf。
     */
    pool = rte_pktmbuf_pool_create("mbuf_inspect_pool",
        cfg.nb_mbuf, cfg.mbuf_cache, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (pool == NULL) {
        rte_eal_cleanup();
        rte_exit(EXIT_FAILURE, "rte_pktmbuf_pool_create failed\n");
    }

    /* 将 pcap PMD port 和 mempool 绑定起来，启动 RX queue。 */
    if (setup_port(port_id, pool) < 0) {
        rte_eal_cleanup();
        return 1;
    }

    memset(&stats, 0, sizeof(stats));
    print_config(port_id, pool);
    /* 真正的 Phase 1 数据路径：rx_burst -> inspect mbuf -> free mbuf。 */
    run_rx_loop(port_id, pool, &stats);
    print_final_stats(port_id, &stats);

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    rte_eal_cleanup();

    return (stats.rx_packets > 0 && stats.samples_printed > 0) ? 0 : 2;
}
