/* SPDX-License-Identifier: BSD-3-Clause
 *
 * DPDK flow pipeline Phase 1：pcap PMD 输入、rte_hash 精确匹配、动作执行和
 * 决策尾延迟统计。硬件 rte_flow 与真实 RSS 不在本阶段伪造。
 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#include "flow_pipeline.h"
#include "flow_capability.h"
#include "flow_worker.h"

#define DEFAULT_NB_MBUF 8192U
#define DEFAULT_MBUF_CACHE 250U
#define DEFAULT_BURST_SIZE 32U
#define DEFAULT_MAX_IDLE_POLLS 100000U
#define MAX_BURST_SIZE 256U

struct app_config {
    uint32_t nb_mbuf;
    uint32_t mbuf_cache;
    uint16_t burst_size;
    uint32_t max_idle_polls;
    uint32_t expected_packets;
    uint32_t extra_rules;
    uint16_t in_port;
    uint16_t out_port;
};

struct app_stats {
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t tx_packets;
    uint64_t tx_bytes;
    uint64_t tx_failed;
    uint64_t freed_packets;
};

static struct app_config config = {
    .nb_mbuf = DEFAULT_NB_MBUF,
    .mbuf_cache = DEFAULT_MBUF_CACHE,
    .burst_size = DEFAULT_BURST_SIZE,
    .max_idle_polls = DEFAULT_MAX_IDLE_POLLS,
    .expected_packets = 64,
    .in_port = 0,
    .out_port = 1,
};
static struct app_stats stats;

static int parse_u32(const char *name, const char *value, uint32_t *out)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(value, &end, 0);
    if (errno != 0 || end == value || *end != '\0' || parsed > UINT32_MAX) {
        fprintf(stderr, "invalid %s: %s\n", name, value);
        return -1;
    }
    *out = (uint32_t)parsed;
    return 0;
}

static int parse_app_args(int argc, char **argv)
{
    int index;

    for (index = 1; index < argc; ++index) {
        uint32_t value;

        if (index + 1 >= argc)
            return -1;
        const char *name = argv[index];
        const char *text = argv[index + 1];

        ++index;
        if (parse_u32(name, text, &value) != 0)
            return -1;
        if (strcmp(name, "--burst-size") == 0 &&
            value > 0 && value <= MAX_BURST_SIZE)
            config.burst_size = (uint16_t)value;
        else if (strcmp(name, "--max-idle-polls") == 0 && value > 0)
            config.max_idle_polls = value;
        else if (strcmp(name, "--nb-mbuf") == 0 && value > 0)
            config.nb_mbuf = value;
        else if (strcmp(name, "--mbuf-cache") == 0)
            config.mbuf_cache = value;
        else if (strcmp(name, "--expected-packets") == 0 && value > 0)
            config.expected_packets = value;
        else if (strcmp(name, "--extra-rules") == 0 &&
                 value <= FLOW_TABLE_MAX_RULES - 3U)
            config.extra_rules = value;
        else
            return -1;
    }
    return 0;
}

static int setup_rx_port(uint16_t port_id, struct rte_mempool *pool)
{
    struct rte_eth_conf port_conf;
    uint16_t rx_desc = 1024;
    int ret;

    memset(&port_conf, 0, sizeof(port_conf));
    port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
    ret = rte_eth_dev_configure(port_id, 1, 0, &port_conf);
    if (ret < 0)
        return ret;
    ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &rx_desc, NULL);
    if (ret < 0)
        return ret;
    ret = rte_eth_rx_queue_setup(port_id, 0, rx_desc,
                                 rte_eth_dev_socket_id(port_id), NULL, pool);
    if (ret < 0)
        return ret;
    return rte_eth_dev_start(port_id);
}

static int setup_tx_port(uint16_t port_id)
{
    struct rte_eth_conf port_conf;
    uint16_t tx_desc = 1024;
    int ret;

    memset(&port_conf, 0, sizeof(port_conf));
    ret = rte_eth_dev_configure(port_id, 0, 1, &port_conf);
    if (ret < 0)
        return ret;
    ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, NULL, &tx_desc);
    if (ret < 0)
        return ret;
    ret = rte_eth_tx_queue_setup(port_id, 0, tx_desc,
                                 rte_eth_dev_socket_id(port_id), NULL);
    if (ret < 0)
        return ret;
    return rte_eth_dev_start(port_id);
}

static void execute_decision(struct rte_mbuf *mbuf,
                             const struct flow_decision *decision)
{
    struct rte_mbuf *tx[1] = {mbuf};
    uint32_t length = rte_pktmbuf_pkt_len(mbuf);

    if (decision->action == FLOW_ACTION_DROP) {
        /* DROP 后应用仍持有 mbuf，必须显式归还 mempool。 */
        stats.freed_packets++;
        rte_pktmbuf_free(mbuf);
        return;
    }
    if (decision->action == FLOW_ACTION_MARK)
        mbuf->hash.fdir.hi = decision->mark_id;
    if (rte_eth_tx_burst(decision->out_port, 0, tx, 1) == 1) {
        /* TX 成功后所有权转移给 PMD，应用不能再次访问或释放该 mbuf。 */
        stats.tx_packets++;
        stats.tx_bytes += length;
    } else {
        /* TX 未接收该 mbuf，所有权仍在应用，失败路径负责释放。 */
        stats.tx_failed++;
        rte_pktmbuf_free(mbuf);
    }
}

static void run_loop(struct flow_pipeline *pipeline)
{
    struct rte_mbuf *packets[MAX_BURST_SIZE];
    uint32_t idle_polls = 0;

    for (;;) {
        uint16_t count = rte_eth_rx_burst(config.in_port, 0, packets,
                                          config.burst_size);
        uint16_t index;

        if (count == 0) {
            /* pcap 输入读完后等待固定空轮询次数，避免过早退出遗漏尾包。 */
            if (stats.rx_packets > 0 && ++idle_polls >= config.max_idle_polls)
                break;
            continue;
        }
        idle_polls = 0;
        for (index = 0; index < count; ++index) {
            struct flow_decision decision;

            stats.rx_packets++;
            stats.rx_bytes += rte_pktmbuf_pkt_len(packets[index]);
            flow_pipeline_classify(pipeline, packets[index], &decision);
            execute_decision(packets[index], &decision);
        }
    }
}

int main(int argc, char **argv)
{
    struct rte_mempool *pool = NULL;
    struct flow_pipeline pipeline;
    uint16_t available_ports;
    int eal_args;
    int lifecycle_result;
    int worker_result;
    int rc = EXIT_FAILURE;

    eal_args = rte_eal_init(argc, argv);
    if (eal_args < 0)
        return EXIT_FAILURE;
    argc -= eal_args;
    argv += eal_args;

    /* 将参数、流量模型和端口边界分开报告，便于自动化测试定位失败层。 */
    if (parse_app_args(argc, argv) != 0) {
        puts("FLOW_CONFIG_BOUNDARY_REJECT reason=arguments");
        goto out;
    }
    if (config.expected_packets % 4U != 0 ||
        config.expected_packets > FLOW_LATENCY_MAX_SAMPLES) {
        printf("FLOW_CONFIG_BOUNDARY_REJECT reason=expected_packets value=%u"
               " divisor=4 max=%u\n",
               config.expected_packets, FLOW_LATENCY_MAX_SAMPLES);
        goto out;
    }
    available_ports = rte_eth_dev_count_avail();
    if (available_ports < 2) {
        printf("FLOW_PORT_BOUNDARY_REJECT available=%u required=2\n",
               available_ports);
        goto out;
    }

    pool = rte_pktmbuf_pool_create("flow_pipeline_pool", config.nb_mbuf,
                                   config.mbuf_cache, 0,
                                   RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (pool == NULL || setup_rx_port(config.in_port, pool) < 0 ||
        setup_tx_port(config.out_port) < 0 ||
        flow_pipeline_create(&pipeline, config.out_port) < 0 ||
        flow_table_add_filler_rules(pipeline.table, config.extra_rules,
                                    config.out_port) != 0)
        goto out;

    flow_capability_probe(config.in_port);
    printf("FLOW_CONFIG lcore=%u in_port=%u out_port=%u burst=%u cache=%u"
           " expected=%u extra_rules=%u total_rules=%u\n",
           rte_lcore_id(), config.in_port, config.out_port, config.burst_size,
           config.mbuf_cache, config.expected_packets, config.extra_rules,
           pipeline.table->rule_count);
    run_loop(&pipeline);
    flow_pipeline_print(&pipeline);
    printf("APP_RESULT rx=%" PRIu64 " tx=%" PRIu64
           " tx_failed=%" PRIu64 " freed=%" PRIu64 "\n",
           stats.rx_packets, stats.tx_packets,
           stats.tx_failed, stats.freed_packets);
    lifecycle_result = flow_table_lifecycle_selftest(pipeline.table);
    worker_result = flow_worker_model_selftest(pool);
    if (lifecycle_result == 0 && worker_result == 0 &&
        stats.rx_packets == config.expected_packets &&
        pipeline.stats.hash_hits == config.expected_packets * 3U / 4U &&
        pipeline.stats.hash_misses == config.expected_packets / 4U &&
        pipeline.stats.rule_drops == config.expected_packets / 4U &&
        pipeline.stats.forwards == config.expected_packets / 4U &&
        pipeline.stats.marks == config.expected_packets / 4U &&
        pipeline.stats.default_drops == config.expected_packets / 4U &&
        pipeline.stats.invalid_packets == 0 &&
        stats.tx_packets == config.expected_packets / 2U &&
        stats.tx_failed == 0) {
        puts("PASS_FLOW_HASH_ACTIONS");
        puts("PASS_FLOW_LATENCY_SAMPLES");
        puts("DPDK_FLOW_PIPELINE_PHASE1_PASS");
        puts("DPDK_FLOW_PIPELINE_PHASE2_LIFECYCLE_PASS");
        puts("DPDK_FLOW_PIPELINE_PHASE3_WORKER_PASS");
        rc = EXIT_SUCCESS;
    }
    flow_pipeline_destroy(&pipeline);

out:
    if (rte_eth_dev_is_valid_port(config.in_port))
        rte_eth_dev_stop(config.in_port);
    if (rte_eth_dev_is_valid_port(config.out_port))
        rte_eth_dev_stop(config.out_port);
    rte_eal_cleanup();
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}
