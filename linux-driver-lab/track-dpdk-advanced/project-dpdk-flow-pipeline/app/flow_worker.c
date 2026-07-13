#include "flow_worker.h"

#include <inttypes.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rte_byteorder.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_pause.h>
#include <rte_ring.h>
#include <rte_udp.h>

#include "flow_key.h"
#include "flow_pipeline.h"

#define FLOW_WORKER_COUNT 2U
#define FLOW_WORKER_PACKETS 64U
#define FLOW_WORKER_RING_SIZE 256U
#define FLOW_WORKER_BURST 16U

enum flow_worker_table_mode {
    FLOW_WORKER_TABLE_SHARED = 0,
    FLOW_WORKER_TABLE_SHARDED,
};

struct flow_worker_stats {
    uint64_t packets;
    uint64_t rule_drops;
    uint64_t forwards;
    uint64_t marks;
    uint64_t default_drops;
};

struct flow_worker_context {
    struct rte_ring *ring;
    struct flow_pipeline pipeline;
    struct flow_worker_stats stats;
    const int *stop;
    int launched;
};

static int worker_loop(void *argument)
{
    struct flow_worker_context *context = argument;
    struct rte_mbuf *packets[FLOW_WORKER_BURST];

    for (;;) {
        /* 每个 ring 只有 main producer 和一个 worker consumer，满足 SP/SC。 */
        unsigned int count = rte_ring_dequeue_burst(
            context->ring, (void **)packets, FLOW_WORKER_BURST, NULL);
        unsigned int index;

        if (count == 0) {
            /* stop 只表示不会再入队；ring 排空后 worker 才能安全退出。 */
            if (__atomic_load_n(context->stop, __ATOMIC_ACQUIRE) &&
                rte_ring_empty(context->ring))
                break;
            rte_pause();
            continue;
        }
        for (index = 0; index < count; ++index) {
            struct flow_decision decision;

            flow_pipeline_classify(&context->pipeline, packets[index],
                                   &decision);
            context->stats.packets++;
            if (decision.action == FLOW_ACTION_DROP) {
                if (decision.table_hit)
                    context->stats.rule_drops++;
                else
                    context->stats.default_drops++;
            } else if (decision.action == FLOW_ACTION_FORWARD) {
                context->stats.forwards++;
            } else if (decision.action == FLOW_ACTION_MARK) {
                context->stats.marks++;
            }
            /* worker 自测不发送真实 TX，分类完成后由当前 consumer 释放 mbuf。 */
            rte_pktmbuf_free(packets[index]);
        }
    }
    return 0;
}

static struct rte_mbuf *build_packet(struct rte_mempool *pool,
                                     uint32_t flow_index)
{
    struct rte_mbuf *mbuf;
    struct rte_ether_hdr *ether;
    struct rte_ipv4_hdr *ipv4;
    struct rte_udp_hdr *udp;
    struct flow_key key;
    char *data;
    const uint16_t payload_size = 16;
    const uint16_t packet_size = sizeof(*ether) + sizeof(*ipv4) +
                                 sizeof(*udp) + payload_size;
    uint8_t host = (uint8_t)(flow_index + 1U);
    uint16_t port = (uint16_t)(10001U + flow_index);

    /* 直接从 DPDK mempool 构造 mbuf，避免 worker 测试依赖额外 pcap 文件。 */
    mbuf = rte_pktmbuf_alloc(pool);
    if (mbuf == NULL)
        return NULL;
    data = rte_pktmbuf_append(mbuf, packet_size);
    if (data == NULL) {
        rte_pktmbuf_free(mbuf);
        return NULL;
    }
    memset(data, 0, packet_size);
    ether = (struct rte_ether_hdr *)data;
    ipv4 = (struct rte_ipv4_hdr *)(ether + 1);
    udp = (struct rte_udp_hdr *)(ipv4 + 1);
    flow_key_set_ipv4_udp(&key, 10, 1, 0, host, 10, 20, 0, host,
                          port, (uint16_t)(20001U + flow_index));
    ether->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    ipv4->version_ihl = 0x45;
    ipv4->next_proto_id = IPPROTO_UDP;
    ipv4->src_addr = key.src_addr;
    ipv4->dst_addr = key.dst_addr;
    udp->src_port = key.src_port;
    udp->dst_port = key.dst_port;
    return mbuf;
}

static int run_worker_mode(struct rte_mempool *pool,
                           enum flow_worker_table_mode mode,
                           const unsigned int worker_lcores[FLOW_WORKER_COUNT])
{
    struct flow_worker_context workers[FLOW_WORKER_COUNT];
    struct flow_table shared_table;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t drops = 0;
    uint64_t forwards = 0;
    uint64_t marks = 0;
    uint64_t default_drops = 0;
    const char *mode_name = mode == FLOW_WORKER_TABLE_SHARED ?
                            "shared" : "sharded";
    int stop = 0;
    int shared_created = 0;
    int rc = -1;
    uint32_t index;

    memset(workers, 0, sizeof(workers));
    memset(&shared_table, 0, sizeof(shared_table));
    if (mode == FLOW_WORKER_TABLE_SHARED) {
        /* shared 模式只创建一张表，worker 运行期间禁止控制面修改。 */
        if (flow_table_create(&shared_table, 1) != 0)
            goto out;
        shared_created = 1;
    }

    for (index = 0; index < FLOW_WORKER_COUNT; ++index) {
        char ring_name[RTE_RING_NAMESIZE];

        snprintf(ring_name, sizeof(ring_name), "flow_%ld_%s_%u",
                 (long)getpid(), mode_name, index);
        workers[index].ring = rte_ring_create(
            ring_name, FLOW_WORKER_RING_SIZE, rte_socket_id(),
            RING_F_SP_ENQ | RING_F_SC_DEQ);
        workers[index].stop = &stop;
        if (workers[index].ring == NULL)
            goto stop_workers;
        if (mode == FLOW_WORKER_TABLE_SHARED)
            flow_pipeline_attach(&workers[index].pipeline, &shared_table);
        else if (flow_pipeline_create(&workers[index].pipeline, 1) != 0)
            goto stop_workers;
        if (rte_eal_remote_launch(worker_loop, &workers[index],
                                  worker_lcores[index]) != 0)
            goto stop_workers;
        workers[index].launched = 1;
    }

    for (index = 0; index < FLOW_WORKER_PACKETS; ++index) {
        struct rte_mbuf *mbuf = build_packet(pool, index % 4U);
        struct flow_key key;
        uint32_t queue;

        if (mbuf == NULL || flow_key_extract(mbuf, &key) != 0) {
            if (mbuf != NULL)
                rte_pktmbuf_free(mbuf);
            goto stop_workers;
        }
        /* 软件分流仅用于当前单 RX queue PMD，使用源地址最低位保证均衡。 */
        queue = rte_be_to_cpu_32(key.src_addr) & 1U;
        if (rte_ring_enqueue(workers[queue].ring, mbuf) != 0) {
            rte_pktmbuf_free(mbuf);
            goto stop_workers;
        }
    }
    rc = 0;

stop_workers:
    /* release 发布 stop，worker acquire 后仍会继续 drain 已入队的 mbuf。 */
    __atomic_store_n(&stop, 1, __ATOMIC_RELEASE);
    for (index = 0; index < FLOW_WORKER_COUNT; ++index) {
        if (workers[index].launched)
            rte_eal_wait_lcore(worker_lcores[index]);
    }
    if (rc != 0)
        goto out;

    for (index = 0; index < FLOW_WORKER_COUNT; ++index) {
        hits += workers[index].pipeline.stats.hash_hits;
        misses += workers[index].pipeline.stats.hash_misses;
        drops += workers[index].stats.rule_drops;
        forwards += workers[index].stats.forwards;
        marks += workers[index].stats.marks;
        default_drops += workers[index].stats.default_drops;
    }
    printf("FLOW_WORKER_RESULT mode=%s queue0=%" PRIu64
           " queue1=%" PRIu64 " hits=%" PRIu64 " misses=%" PRIu64
           " drop=%" PRIu64 " forward=%" PRIu64 " mark=%" PRIu64
           " default_drop=%" PRIu64 "\n",
           mode_name, workers[0].stats.packets, workers[1].stats.packets,
           hits, misses, drops, forwards, marks, default_drops);
    if (workers[0].stats.packets != 32 || workers[1].stats.packets != 32 ||
        hits != 48 || misses != 16 || drops != 16 || forwards != 16 ||
        marks != 16 || default_drops != 16)
        rc = -1;

out:
    /* attached pipeline 不销毁 shared table；owner 在所有 worker 退出后统一销毁。 */
    for (index = 0; index < FLOW_WORKER_COUNT; ++index) {
        flow_pipeline_destroy(&workers[index].pipeline);
        if (workers[index].ring != NULL)
            rte_ring_free(workers[index].ring);
    }
    if (shared_created)
        flow_table_destroy(&shared_table);
    return rc;
}

int flow_worker_model_selftest(struct rte_mempool *pool)
{
    unsigned int worker_lcores[FLOW_WORKER_COUNT];
    unsigned int lcore_id;
    uint32_t count = 0;

    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (count < FLOW_WORKER_COUNT)
            worker_lcores[count++] = lcore_id;
    }
    if (count < FLOW_WORKER_COUNT) {
        printf("FLOW_WORKER_BLOCKED available=%u required=%u\n",
               count, FLOW_WORKER_COUNT);
        return -1;
    }
    if (run_worker_mode(pool, FLOW_WORKER_TABLE_SHARED, worker_lcores) != 0)
        return -1;
    puts("FLOW_WORKER_SHARED_TABLE_PASS");
    if (run_worker_mode(pool, FLOW_WORKER_TABLE_SHARDED, worker_lcores) != 0)
        return -1;
    puts("FLOW_WORKER_SHARDED_TABLE_PASS");
    puts("DPDK_FLOW_PIPELINE_PHASE3_PASS");
    return 0;
}
