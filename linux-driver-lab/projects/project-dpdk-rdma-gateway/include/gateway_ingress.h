#ifndef GATEWAY_INGRESS_H
#define GATEWAY_INGRESS_H

#include <stdint.h>

#include <rte_mbuf.h>

#include "gateway_contract.h"

struct gateway_ingress_stats {
    uint64_t rx_packets;
    uint64_t udp_packets;
    uint64_t unsupported_packets;
    uint64_t malformed_packets;
    uint64_t staged_requests;
    uint64_t ring_full;
    uint64_t slot_exhausted;
};

struct gateway_backend_stats {
    uint64_t dequeued_requests;
    uint64_t completed_requests;
    uint64_t payload_bytes;
};

/* Phase 2 把 staging、slot 元数据和 request ring 放在同一个测试上下文。 */
struct gateway_ingress_context {
    struct gateway_staging_slot staging[GATEWAY_SLOT_COUNT];
    struct gateway_slot_pool slot_pool;
    struct gateway_request_ring request_ring;
    struct gateway_ingress_stats stats;
    uint64_t next_request_id;
    uint32_t slot_cursor;
};

void gateway_ingress_init(struct gateway_ingress_context *context);
int gateway_ingress_process(struct gateway_ingress_context *context,
                            const struct rte_mbuf *mbuf, uint16_t ingress_port,
                            uint16_t rx_queue);
int gateway_mock_rdma_drain(struct gateway_ingress_context *context,
                            struct gateway_backend_stats *stats);

#endif
