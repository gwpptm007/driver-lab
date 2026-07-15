#include "gateway_contract.h"

#include <errno.h>
#include <string.h>

enum gateway_wire_offset {
    WIRE_MAGIC = 0,
    WIRE_VERSION = 4,
    WIRE_HEADER_LEN = 6,
    WIRE_OPCODE = 8,
    WIRE_FLAGS = 9,
    WIRE_INGRESS_PORT = 10,
    WIRE_RX_QUEUE = 12,
    WIRE_RESERVED = 14,
    WIRE_REQUEST_ID = 16,
    WIRE_FLOW_HASH = 24,
    WIRE_PAYLOAD_LEN = 32,
    WIRE_GENERATION = 36,
};

_Static_assert(sizeof(struct gateway_request) == 32,
               "gateway request layout changed");
_Static_assert(sizeof(struct gateway_staging_slot) == GATEWAY_MAX_PAYLOAD,
               "staging slot layout changed");

/* 显式按大端写入，禁止把有 padding 的本地主机结构体直接发送到远端。 */
static void put_be16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value >> 8);
    output[1] = (uint8_t)value;
}

static void put_be32(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value >> 24);
    output[1] = (uint8_t)(value >> 16);
    output[2] = (uint8_t)(value >> 8);
    output[3] = (uint8_t)value;
}

static void put_be64(uint8_t *output, uint64_t value)
{
    put_be32(output, (uint32_t)(value >> 32));
    put_be32(output + 4, (uint32_t)value);
}

static uint16_t get_be16(const uint8_t *input)
{
    return (uint16_t)(((uint16_t)input[0] << 8) | input[1]);
}

static uint32_t get_be32(const uint8_t *input)
{
    return ((uint32_t)input[0] << 24) | ((uint32_t)input[1] << 16) |
           ((uint32_t)input[2] << 8) | input[3];
}

static uint64_t get_be64(const uint8_t *input)
{
    return ((uint64_t)get_be32(input) << 32) | get_be32(input + 4);
}

int gateway_request_validate(const struct gateway_request *request)
{
    if (request == NULL || request->opcode != GATEWAY_OP_RDMA_WRITE)
        return -EINVAL;
    if (request->slot_id >= GATEWAY_SLOT_COUNT || request->generation == 0)
        return -ERANGE;
    if (request->payload_len == 0 ||
        request->payload_len > GATEWAY_MAX_PAYLOAD)
        return -EMSGSIZE;
    return 0;
}

int gateway_wire_encode(const struct gateway_request *request,
                        uint8_t *output, size_t output_size)
{
    int ret = gateway_request_validate(request);

    if (ret != 0)
        return ret;
    if (output == NULL || output_size < GATEWAY_WIRE_HEADER_SIZE)
        return -ENOBUFS;

    memset(output, 0, GATEWAY_WIRE_HEADER_SIZE);
    put_be32(output + WIRE_MAGIC, GATEWAY_WIRE_MAGIC);
    put_be16(output + WIRE_VERSION, GATEWAY_WIRE_VERSION);
    put_be16(output + WIRE_HEADER_LEN, GATEWAY_WIRE_HEADER_SIZE);
    output[WIRE_OPCODE] = request->opcode;
    output[WIRE_FLAGS] = request->flags;
    put_be16(output + WIRE_INGRESS_PORT, request->ingress_port);
    put_be16(output + WIRE_RX_QUEUE, request->rx_queue);
    put_be16(output + WIRE_RESERVED, 0);
    put_be64(output + WIRE_REQUEST_ID, request->request_id);
    put_be64(output + WIRE_FLOW_HASH, request->flow_hash);
    put_be32(output + WIRE_PAYLOAD_LEN, request->payload_len);
    put_be32(output + WIRE_GENERATION, request->generation);
    return 0;
}

int gateway_wire_decode(const uint8_t *input, size_t input_size,
                        struct gateway_request *request)
{
    if (input == NULL || request == NULL ||
        input_size < GATEWAY_WIRE_HEADER_SIZE)
        return -ENOBUFS;
    if (get_be32(input + WIRE_MAGIC) != GATEWAY_WIRE_MAGIC ||
        get_be16(input + WIRE_VERSION) != GATEWAY_WIRE_VERSION ||
        get_be16(input + WIRE_HEADER_LEN) != GATEWAY_WIRE_HEADER_SIZE ||
        get_be16(input + WIRE_RESERVED) != 0)
        return -EPROTO;

    memset(request, 0, sizeof(*request));
    request->request_id = get_be64(input + WIRE_REQUEST_ID);
    request->flow_hash = get_be64(input + WIRE_FLOW_HASH);
    request->generation = get_be32(input + WIRE_GENERATION);
    request->payload_len = (uint16_t)get_be32(input + WIRE_PAYLOAD_LEN);
    request->ingress_port = get_be16(input + WIRE_INGRESS_PORT);
    request->rx_queue = get_be16(input + WIRE_RX_QUEUE);
    request->opcode = input[WIRE_OPCODE];
    request->flags = input[WIRE_FLAGS];

    /* slot_id 只在本地 descriptor 中有效，远端解码后由接收端自行分配。 */
    request->slot_id = 0;
    return gateway_request_validate(request);
}

void gateway_ring_init(struct gateway_request_ring *ring)
{
    memset(ring->entries, 0, sizeof(ring->entries));
    atomic_init(&ring->producer, 0);
    atomic_init(&ring->consumer, 0);
}

int gateway_ring_enqueue(struct gateway_request_ring *ring,
                         const struct gateway_request *request)
{
    uint32_t producer = atomic_load_explicit(&ring->producer,
                                             memory_order_relaxed);
    uint32_t consumer = atomic_load_explicit(&ring->consumer,
                                             memory_order_acquire);

    if (producer - consumer >= GATEWAY_RING_CAPACITY)
        return -ENOSPC;
    ring->entries[producer % GATEWAY_RING_CAPACITY] = *request;
    /* release 保证 descriptor 内容先于 producer 索引对 consumer 可见。 */
    atomic_store_explicit(&ring->producer, producer + 1,
                          memory_order_release);
    return 0;
}

int gateway_ring_dequeue(struct gateway_request_ring *ring,
                         struct gateway_request *request)
{
    uint32_t consumer = atomic_load_explicit(&ring->consumer,
                                             memory_order_relaxed);
    uint32_t producer = atomic_load_explicit(&ring->producer,
                                             memory_order_acquire);

    if (consumer == producer)
        return -ENOENT;
    *request = ring->entries[consumer % GATEWAY_RING_CAPACITY];
    atomic_store_explicit(&ring->consumer, consumer + 1,
                          memory_order_release);
    return 0;
}

void gateway_slot_pool_init(struct gateway_slot_pool *pool)
{
    uint32_t slot_id;

    memset(pool, 0, sizeof(*pool));
    for (slot_id = 0; slot_id < GATEWAY_SLOT_COUNT; ++slot_id)
        atomic_init(&pool->slots[slot_id].phase, GATEWAY_SLOT_FREE);
}

int gateway_slot_prepare(struct gateway_slot_pool *pool, uint32_t slot_id,
                         uint16_t payload_len, uint32_t *generation)
{
    struct gateway_slot_meta *slot;

    if (slot_id >= GATEWAY_SLOT_COUNT || generation == NULL)
        return -ERANGE;
    if (payload_len == 0 || payload_len > GATEWAY_MAX_PAYLOAD)
        return -EMSGSIZE;
    slot = &pool->slots[slot_id];
    /* 单 producer 在看到 consumer release 的 FREE 后才重写 slot 元数据。 */
    if (atomic_load_explicit(&slot->phase, memory_order_acquire) !=
        GATEWAY_SLOT_FREE)
        return -EBUSY;

    slot->generation++;
    if (slot->generation == 0)
        slot->generation = 1;
    slot->payload_len = payload_len;
    atomic_store_explicit(&slot->phase, GATEWAY_SLOT_READY,
                          memory_order_release);
    *generation = slot->generation;
    return 0;
}

int gateway_slot_prepare_next(struct gateway_slot_pool *pool, uint32_t *cursor,
                              uint16_t payload_len, uint32_t *slot_id,
                              uint32_t *generation)
{
    uint32_t offset;

    if (cursor == NULL || slot_id == NULL || generation == NULL)
        return -EINVAL;
    for (offset = 0; offset < GATEWAY_SLOT_COUNT; ++offset) {
        uint32_t candidate = (*cursor + offset) % GATEWAY_SLOT_COUNT;
        int ret = gateway_slot_prepare(pool, candidate, payload_len,
                                       generation);

        if (ret == 0) {
            *slot_id = candidate;
            *cursor = (candidate + 1) % GATEWAY_SLOT_COUNT;
            return 0;
        }
        if (ret != -EBUSY)
            return ret;
    }
    return -ENOSPC;
}

int gateway_slot_cancel_ready(struct gateway_slot_pool *pool, uint32_t slot_id,
                              uint32_t generation)
{
    struct gateway_slot_meta *slot;

    if (slot_id >= GATEWAY_SLOT_COUNT)
        return -ERANGE;
    slot = &pool->slots[slot_id];
    if (atomic_load_explicit(&slot->phase, memory_order_acquire) !=
            GATEWAY_SLOT_READY ||
        slot->generation != generation)
        return -ESTALE;
    slot->payload_len = 0;
    atomic_store_explicit(&slot->phase, GATEWAY_SLOT_FREE,
                          memory_order_release);
    return 0;
}

int gateway_slot_mark_inflight(struct gateway_slot_pool *pool,
                               uint32_t slot_id, uint32_t generation)
{
    struct gateway_slot_meta *slot;

    if (slot_id >= GATEWAY_SLOT_COUNT)
        return -ERANGE;
    slot = &pool->slots[slot_id];
    if (atomic_load_explicit(&slot->phase, memory_order_acquire) !=
            GATEWAY_SLOT_READY ||
        slot->generation != generation)
        return -ESTALE;
    atomic_store_explicit(&slot->phase, GATEWAY_SLOT_INFLIGHT,
                          memory_order_release);
    return 0;
}

int gateway_slot_complete(struct gateway_slot_pool *pool, uint32_t slot_id,
                          uint32_t generation)
{
    struct gateway_slot_meta *slot;

    if (slot_id >= GATEWAY_SLOT_COUNT)
        return -ERANGE;
    slot = &pool->slots[slot_id];
    if (atomic_load_explicit(&slot->phase, memory_order_acquire) !=
            GATEWAY_SLOT_INFLIGHT ||
        slot->generation != generation)
        return -ESTALE;

    /* completion 匹配当前 generation 后才能重新开放 slot。 */
    slot->payload_len = 0;
    atomic_store_explicit(&slot->phase, GATEWAY_SLOT_FREE,
                          memory_order_release);
    return 0;
}

uint32_t gateway_slot_count_phase(const struct gateway_slot_pool *pool,
                                  enum gateway_slot_phase phase)
{
    uint32_t count = 0;
    uint32_t slot_id;

    for (slot_id = 0; slot_id < GATEWAY_SLOT_COUNT; ++slot_id) {
        if (atomic_load_explicit(&pool->slots[slot_id].phase,
                                 memory_order_acquire) == phase)
            count++;
    }
    return count;
}
