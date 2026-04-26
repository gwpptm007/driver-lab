/*
 * Minimal DPDK L2 forwarding skeleton.
 *
 * This file is intentionally small and educational.
 * Fill in build/run details after Phase 1~3 are validated.
 */

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <inttypes.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#define NB_MBUF     8192
#define MBUF_CACHE  250
#define BURST_SIZE  32

static volatile bool force_quit;

static void handle_signal(int sig)
{
    (void)sig;
    force_quit = true;
}

int main(int argc, char **argv)
{
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eal_init failed\n");
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    uint16_t nb_ports = rte_eth_dev_count_avail();
    printf("available ports: %u\n", nb_ports);

    /*
     * TODO:
     * 1. create mbuf pool
     * 2. configure one or two ports
     * 3. setup rx/tx queues
     * 4. start ports
     * 5. rx_burst -> tx_burst loop
     * 6. stats and cleanup
     */

    while (!force_quit) {
        /* TODO: RX/TX burst loop */
        break;
    }

    printf("bye\n");
    return 0;
}
