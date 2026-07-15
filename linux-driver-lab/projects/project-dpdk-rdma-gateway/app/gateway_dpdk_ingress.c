#include "gateway_ingress.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#define GATEWAY_BURST_SIZE 32U
#define GATEWAY_NB_MBUF 8192U
#define GATEWAY_MBUF_CACHE 250U
#define GATEWAY_MAX_IDLE_POLLS 100000U

static int setup_rx_port(uint16_t port_id, struct rte_mempool *pool)
{
    struct rte_eth_conf config;
    uint16_t rx_desc = 1024;
    int ret;

    memset(&config, 0, sizeof(config));
    config.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
    ret = rte_eth_dev_configure(port_id, 1, 0, &config);
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

static int parse_expected_packets(int argc, char **argv, uint32_t *expected)
{
    if (argc != 3 || strcmp(argv[1], "--expected-packets") != 0)
        return -1;
    *expected = (uint32_t)strtoul(argv[2], NULL, 10);
    return *expected == 0 ? -1 : 0;
}

int main(int argc, char **argv)
{
    struct gateway_ingress_context *context = NULL;
    struct gateway_backend_stats backend;
    struct rte_mempool *pool = NULL;
    struct rte_mbuf *packets[GATEWAY_BURST_SIZE];
    uint32_t expected_packets = 0;
    uint32_t idle_polls = 0;
    uint16_t port_id = 0;
    int eal_args;
    int rc = EXIT_FAILURE;

    eal_args = rte_eal_init(argc, argv);
    if (eal_args < 0)
        return EXIT_FAILURE;
    argc -= eal_args;
    argv += eal_args;
    if (parse_expected_packets(argc, argv, &expected_packets) != 0 ||
        rte_eth_dev_count_avail() < 1) {
        puts("GATEWAY_INGRESS_BOUNDARY_REJECT reason=config_or_port");
        goto out;
    }

    context = calloc(1, sizeof(*context));
    pool = rte_pktmbuf_pool_create("gateway_ingress_pool", GATEWAY_NB_MBUF,
                                   GATEWAY_MBUF_CACHE, 0,
                                   RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (context == NULL || pool == NULL || setup_rx_port(port_id, pool) < 0)
        goto out;
    gateway_ingress_init(context);
    printf("GATEWAY_INGRESS_CONFIG port=%u queue=0 expected=%u burst=%u\n",
           port_id, expected_packets, GATEWAY_BURST_SIZE);

    while (context->stats.rx_packets < expected_packets &&
           idle_polls < GATEWAY_MAX_IDLE_POLLS) {
        uint16_t count = rte_eth_rx_burst(port_id, 0, packets,
                                          GATEWAY_BURST_SIZE);
        uint16_t index;

        if (count == 0) {
            idle_polls++;
            continue;
        }
        idle_polls = 0;
        for (index = 0; index < count; ++index) {
            /* payload 已复制进 staging，原 mbuf 可以立即归还 DPDK mempool。 */
            gateway_ingress_process(context, packets[index], port_id, 0);
            rte_pktmbuf_free(packets[index]);
        }
    }

    if (gateway_mock_rdma_drain(context, &backend) != 0)
        goto out;
    printf("GATEWAY_INGRESS_RESULT rx=%" PRIu64 " udp=%" PRIu64
           " unsupported=%" PRIu64 " malformed=%" PRIu64
           " staged=%" PRIu64 " ring_full=%" PRIu64
           " slot_exhausted=%" PRIu64 "\n",
           context->stats.rx_packets, context->stats.udp_packets,
           context->stats.unsupported_packets,
           context->stats.malformed_packets,
           context->stats.staged_requests, context->stats.ring_full,
           context->stats.slot_exhausted);
    printf("GATEWAY_MOCK_RDMA_RESULT dequeued=%" PRIu64
           " completed=%" PRIu64 " payload_bytes=%" PRIu64 "\n",
           backend.dequeued_requests, backend.completed_requests,
           backend.payload_bytes);

    if (context->stats.rx_packets == expected_packets &&
        context->stats.udp_packets == expected_packets * 3U / 4U &&
        context->stats.unsupported_packets == expected_packets / 4U &&
        context->stats.malformed_packets == 0 &&
        context->stats.staged_requests == expected_packets * 3U / 4U &&
        context->stats.ring_full == 0 &&
        context->stats.slot_exhausted == 0 &&
        backend.dequeued_requests == expected_packets * 3U / 4U &&
        backend.completed_requests == expected_packets * 3U / 4U &&
        backend.payload_bytes == expected_packets * 3U / 4U * 32U) {
        puts("DPDK_RDMA_GATEWAY_PHASE2_INGRESS_PASS");
        rc = EXIT_SUCCESS;
    }

out:
    if (rte_eth_dev_is_valid_port(port_id))
        rte_eth_dev_stop(port_id);
    if (pool != NULL)
        rte_mempool_free(pool);
    free(context);
    rte_eal_cleanup();
    printf("cleanup=complete result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}
