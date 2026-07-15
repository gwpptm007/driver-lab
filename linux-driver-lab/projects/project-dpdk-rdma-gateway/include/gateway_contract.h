#ifndef GATEWAY_CONTRACT_H
#define GATEWAY_CONTRACT_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#define GATEWAY_WIRE_MAGIC 0x44524757U
#define GATEWAY_WIRE_VERSION 1U
#define GATEWAY_WIRE_HEADER_SIZE 40U
#define GATEWAY_MAX_PAYLOAD 2048U
#define GATEWAY_RING_CAPACITY 64U
#define GATEWAY_SLOT_COUNT 64U

enum gateway_opcode {
    GATEWAY_OP_RDMA_WRITE = 1,
};

/*
 * DPDK producer 与 RDMA consumer 之间只传固定 32 字节描述符。
 * slot_id 是本地 staging 索引，不进入远端 wire header。
 */
struct gateway_request {
    uint64_t request_id;
    uint64_t flow_hash;
    uint32_t slot_id;
    uint32_t generation;
    uint16_t payload_len;
    uint16_t ingress_port;
    uint16_t rx_queue;
    uint8_t opcode;
    uint8_t flags;
};

/* 每个 slot 独占一个标准 MTU 级 payload，并按 cache line 对齐。 */
struct gateway_staging_slot {
    _Alignas(64) uint8_t payload[GATEWAY_MAX_PAYLOAD];
};

/* 单 producer / 单 consumer ring；单调计数器避免浪费一个数组槽位。 */
struct gateway_request_ring {
    struct gateway_request entries[GATEWAY_RING_CAPACITY];
    _Atomic uint32_t producer;
    _Atomic uint32_t consumer;
};

enum gateway_slot_phase {
    GATEWAY_SLOT_FREE = 0,
    GATEWAY_SLOT_READY,
    GATEWAY_SLOT_INFLIGHT,
};

struct gateway_slot_meta {
    uint32_t generation;
    uint16_t payload_len;
    _Atomic uint8_t phase;
};

struct gateway_slot_pool {
    struct gateway_slot_meta slots[GATEWAY_SLOT_COUNT];
};

int gateway_request_validate(const struct gateway_request *request);
int gateway_wire_encode(const struct gateway_request *request,
                        uint8_t *output, size_t output_size);
int gateway_wire_decode(const uint8_t *input, size_t input_size,
                        struct gateway_request *request);

void gateway_ring_init(struct gateway_request_ring *ring);
int gateway_ring_enqueue(struct gateway_request_ring *ring,
                         const struct gateway_request *request);
int gateway_ring_dequeue(struct gateway_request_ring *ring,
                         struct gateway_request *request);

void gateway_slot_pool_init(struct gateway_slot_pool *pool);
int gateway_slot_prepare(struct gateway_slot_pool *pool, uint32_t slot_id,
                         uint16_t payload_len, uint32_t *generation);
int gateway_slot_prepare_next(struct gateway_slot_pool *pool, uint32_t *cursor,
                              uint16_t payload_len, uint32_t *slot_id,
                              uint32_t *generation);
int gateway_slot_cancel_ready(struct gateway_slot_pool *pool, uint32_t slot_id,
                              uint32_t generation);
int gateway_slot_mark_inflight(struct gateway_slot_pool *pool,
                               uint32_t slot_id, uint32_t generation);
int gateway_slot_complete(struct gateway_slot_pool *pool, uint32_t slot_id,
                          uint32_t generation);
uint32_t gateway_slot_count_phase(const struct gateway_slot_pool *pool,
                                  enum gateway_slot_phase phase);

#endif
