#include "gateway_ingress.h"
#include "gateway_rdma_backend.h"
#include "gateway_rdma_worker.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#define GATEWAY_E2E_BURST 32U
#define GATEWAY_E2E_NB_MBUF 8192U
#define GATEWAY_E2E_MBUF_CACHE 250U
#define GATEWAY_E2E_IDLE_POLLS 100000U

struct gateway_e2e_config {
    struct rdma_cs_options rdma;
    uint32_t expected_packets;
};

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

static int parse_app_args(int argc, char **argv,
                          struct gateway_e2e_config *config)
{
    int index;

    rdma_cs_options_init(&config->rdma);
    config->expected_packets = 64;
    for (index = 1; index < argc; index += 2) {
        if (index + 1 >= argc)
            return -1;
        if (strcmp(argv[index], "--expected-packets") == 0)
            config->expected_packets = (uint32_t)strtoul(argv[index + 1],
                                                         NULL, 10);
        else if (strcmp(argv[index], "--server") == 0)
            config->rdma.server_addr = argv[index + 1];
        else if (strcmp(argv[index], "--port") == 0)
            config->rdma.tcp_port = argv[index + 1];
        else if (strcmp(argv[index], "--device") == 0)
            config->rdma.device_name = argv[index + 1];
        else if (strcmp(argv[index], "--ib-port") == 0)
            config->rdma.ib_port = atoi(argv[index + 1]);
        else if (strcmp(argv[index], "--gid-index") == 0)
            config->rdma.gid_index = atoi(argv[index + 1]);
        else
            return -1;
    }
    return config->expected_packets == 64 ? 0 : -1;
}

int main(int argc, char **argv)
{
    struct gateway_ingress_context *ingress = NULL;
    struct gateway_rdma_backend backend;
    struct gateway_rdma_worker worker;
    struct gateway_e2e_config config;
    struct rte_mempool *pool = NULL;
    struct rte_mbuf *packets[GATEWAY_E2E_BURST];
    pthread_t worker_thread;
    char line[RDMA_CS_LINE_SIZE];
    uint32_t idle_polls = 0;
    uint32_t active_slots;
    uint16_t port_id = 0;
    int control_fd = -1;
    int worker_started = 0;
    int eal_args;
    int rc = EXIT_FAILURE;

    memset(&backend, 0, sizeof(backend));
    eal_args = rte_eal_init(argc, argv);
    if (eal_args < 0)
        return EXIT_FAILURE;
    argc -= eal_args;
    argv += eal_args;
    if (parse_app_args(argc, argv, &config) != 0 ||
        rte_eth_dev_count_avail() < 1)
        goto out;

    ingress = calloc(1, sizeof(*ingress));
    pool = rte_pktmbuf_pool_create("gateway_e2e_pool", GATEWAY_E2E_NB_MBUF,
                                   GATEWAY_E2E_MBUF_CACHE, 0,
                                   RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (ingress == NULL || pool == NULL ||
        setup_rx_port(port_id, pool) < 0)
        goto out;
    gateway_ingress_init(ingress);

    if (gateway_rdma_backend_create(&backend, &config.rdma,
                                    RDMA_CS_ROLE_CLIENT, 0x45678U) != 0)
        goto out;
    control_fd = rdma_cs_tcp_connect(config.rdma.server_addr,
                                     config.rdma.tcp_port);
    if (control_fd < 0 ||
        gateway_rdma_backend_connect(&backend, control_fd) != 0)
        goto out;
    puts("GATEWAY_E2E_QP_RTS_PASS");

    /* worker 独占 verbs QP/CQ，DPDK 主线程只负责 staging 和 ring 发布。 */
    gateway_rdma_worker_init(&worker, ingress, &backend);
    if (pthread_create(&worker_thread, NULL, gateway_rdma_worker_run,
                       &worker) != 0)
        goto out;
    worker_started = 1;

    while (ingress->stats.rx_packets < config.expected_packets &&
           idle_polls < GATEWAY_E2E_IDLE_POLLS) {
        uint16_t count = rte_eth_rx_burst(port_id, 0, packets,
                                          GATEWAY_E2E_BURST);
        uint16_t index;

        if (count == 0) {
            idle_polls++;
            continue;
        }
        idle_polls = 0;
        for (index = 0; index < count; ++index) {
            gateway_ingress_process(ingress, packets[index], port_id, 0);
            rte_pktmbuf_free(packets[index]);
        }
    }

    /* producer 停止后等待 worker 排空 ring，确保所有 CQE 已回收 slot。 */
    gateway_rdma_worker_stop(&worker);
    pthread_join(worker_thread, NULL);
    worker_started = 0;
    active_slots = GATEWAY_SLOT_COUNT - gateway_slot_count_phase(
        &ingress->slot_pool, GATEWAY_SLOT_FREE);

    printf("GATEWAY_E2E_INGRESS_RESULT rx=%" PRIu64 " udp=%" PRIu64
           " unsupported=%" PRIu64 " staged=%" PRIu64
           " ring_full=%" PRIu64 " slot_exhausted=%" PRIu64 "\n",
           ingress->stats.rx_packets, ingress->stats.udp_packets,
           ingress->stats.unsupported_packets,
           ingress->stats.staged_requests, ingress->stats.ring_full,
           ingress->stats.slot_exhausted);
    printf("GATEWAY_E2E_RDMA_RESULT dequeued=%" PRIu64
           " completed=%" PRIu64 " payload_bytes=%" PRIu64
           " write_bytes=%" PRIu64 " errors=%" PRIu64
           " active_slots=%u\n",
           worker.stats.dequeued_requests, worker.stats.completed_requests,
           worker.stats.payload_bytes, worker.stats.write_bytes,
           worker.stats.errors, active_slots);

    if (ingress->stats.rx_packets != 64 || ingress->stats.udp_packets != 48 ||
        ingress->stats.unsupported_packets != 16 ||
        ingress->stats.malformed_packets != 0 ||
        ingress->stats.staged_requests != 48 ||
        ingress->stats.ring_full != 0 ||
        ingress->stats.slot_exhausted != 0 ||
        worker.stats.dequeued_requests != 48 ||
        worker.stats.completed_requests != 48 ||
        worker.stats.payload_bytes != 1536 ||
        worker.stats.write_bytes != 3456 || worker.stats.errors != 0 ||
        active_slots != 0)
        goto out;

    if (rdma_cs_send_line(control_fd,
                          "BATCH_DONE requests=48 last=48 bytes=3456\n") != 0 ||
        rdma_cs_recv_line(control_fd, line, sizeof(line)) != 0 ||
        strcmp(line, "E2E_REMOTE_VALIDATED\n") != 0)
        goto out;
    puts("DPDK_RDMA_GATEWAY_PHASE4_E2E_PASS");
    rc = EXIT_SUCCESS;

out:
    if (worker_started) {
        gateway_rdma_worker_stop(&worker);
        pthread_join(worker_thread, NULL);
    }
    rdma_cs_close_fd(control_fd);
    gateway_rdma_backend_destroy(&backend);
    if (rte_eth_dev_is_valid_port(port_id))
        rte_eth_dev_stop(port_id);
    if (pool != NULL)
        rte_mempool_free(pool);
    free(ingress);
    rte_eal_cleanup();
    printf("cleanup=complete role=e2e_client result=%s\n",
           rc == EXIT_SUCCESS ? "pass" : "fail");
    return rc;
}
