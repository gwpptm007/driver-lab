#include "gateway_contract.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "CHECK_FAIL line=%d condition=%s\n", __LINE__,    \
                    #condition);                                                \
            return 1;                                                           \
        }                                                                       \
    } while (0)

static struct gateway_request sample_request(uint64_t request_id)
{
    struct gateway_request request = {
        .request_id = request_id,
        .flow_hash = 0x0102030405060708ULL,
        .slot_id = 7,
        .generation = 3,
        .payload_len = 512,
        .ingress_port = 1,
        .rx_queue = 2,
        .opcode = GATEWAY_OP_RDMA_WRITE,
        .flags = 1,
    };

    return request;
}

static int test_wire_contract(void)
{
    struct gateway_request source = sample_request(0x1122334455667788ULL);
    struct gateway_request decoded;
    uint8_t wire[GATEWAY_WIRE_HEADER_SIZE];
    uint8_t invalid[GATEWAY_WIRE_HEADER_SIZE];

    CHECK(sizeof(source) == 32);
    CHECK(_Alignof(struct gateway_staging_slot) == 64);
    CHECK(gateway_wire_encode(&source, wire, sizeof(wire)) == 0);
    CHECK(gateway_wire_decode(wire, sizeof(wire), &decoded) == 0);
    CHECK(decoded.request_id == source.request_id);
    CHECK(decoded.flow_hash == source.flow_hash);
    CHECK(decoded.generation == source.generation);
    CHECK(decoded.payload_len == source.payload_len);
    CHECK(decoded.ingress_port == source.ingress_port);
    CHECK(decoded.rx_queue == source.rx_queue);
    CHECK(decoded.opcode == source.opcode);
    CHECK(decoded.flags == source.flags);
    puts("GATEWAY_CONTRACT_LAYOUT_PASS request_size=32 wire_size=40 align=64");
    puts("GATEWAY_WIRE_ROUNDTRIP_PASS");

    /* 每次只破坏一个字段，确保 decoder 在进入 RDMA 数据面前拒绝。 */
    memcpy(invalid, wire, sizeof(invalid));
    invalid[0] ^= 1;
    CHECK(gateway_wire_decode(invalid, sizeof(invalid), &decoded) == -EPROTO);
    memcpy(invalid, wire, sizeof(invalid));
    invalid[5] = 2;
    CHECK(gateway_wire_decode(invalid, sizeof(invalid), &decoded) == -EPROTO);
    memcpy(invalid, wire, sizeof(invalid));
    invalid[7] = 39;
    CHECK(gateway_wire_decode(invalid, sizeof(invalid), &decoded) == -EPROTO);
    memcpy(invalid, wire, sizeof(invalid));
    invalid[8] = 99;
    CHECK(gateway_wire_decode(invalid, sizeof(invalid), &decoded) == -EINVAL);
    memcpy(invalid, wire, sizeof(invalid));
    invalid[32] = 0;
    invalid[33] = 0;
    invalid[34] = 8;
    invalid[35] = 1;
    CHECK(gateway_wire_decode(invalid, sizeof(invalid), &decoded) == -EMSGSIZE);
    CHECK(gateway_wire_decode(wire, sizeof(wire) - 1, &decoded) == -ENOBUFS);
    puts("GATEWAY_WIRE_BOUNDARY_PASS cases=6");
    return 0;
}

static int test_spsc_ring(void)
{
    struct gateway_request_ring ring;
    struct gateway_request request;
    struct gateway_request output;
    uint64_t sequence;

    gateway_ring_init(&ring);
    CHECK(gateway_ring_dequeue(&ring, &output) == -ENOENT);
    for (sequence = 0; sequence < GATEWAY_RING_CAPACITY; ++sequence) {
        request = sample_request(sequence);
        CHECK(gateway_ring_enqueue(&ring, &request) == 0);
    }
    request = sample_request(999);
    CHECK(gateway_ring_enqueue(&ring, &request) == -ENOSPC);

    for (sequence = 0; sequence < GATEWAY_RING_CAPACITY * 3U; ++sequence) {
        CHECK(gateway_ring_dequeue(&ring, &output) == 0);
        CHECK(output.request_id == sequence);
        request = sample_request(sequence + GATEWAY_RING_CAPACITY);
        CHECK(gateway_ring_enqueue(&ring, &request) == 0);
    }
    for (sequence = GATEWAY_RING_CAPACITY * 3U;
         sequence < GATEWAY_RING_CAPACITY * 4U; ++sequence) {
        CHECK(gateway_ring_dequeue(&ring, &output) == 0);
        CHECK(output.request_id == sequence);
    }
    CHECK(gateway_ring_dequeue(&ring, &output) == -ENOENT);
    puts("GATEWAY_RING_SPSC_PASS requests=256 full_empty_wrap=pass");
    return 0;
}

static int test_slot_lifecycle(void)
{
    struct gateway_slot_pool pool;
    uint32_t cursor = 5;
    uint32_t slot_id;
    uint32_t first_generation;
    uint32_t second_generation;

    gateway_slot_pool_init(&pool);
    CHECK(gateway_slot_prepare_next(&pool, &cursor, 512, &slot_id,
                                    &first_generation) == 0);
    CHECK(slot_id == 5);
    CHECK(first_generation == 1);
    CHECK(gateway_slot_prepare(&pool, 5, 512, &second_generation) == -EBUSY);
    CHECK(gateway_slot_mark_inflight(&pool, 5, first_generation + 1) ==
          -ESTALE);
    CHECK(gateway_slot_mark_inflight(&pool, 5, first_generation) == 0);
    CHECK(gateway_slot_complete(&pool, 5, first_generation + 1) == -ESTALE);
    CHECK(gateway_slot_complete(&pool, 5, first_generation) == 0);
    CHECK(gateway_slot_prepare(&pool, 5, 128, &second_generation) == 0);
    CHECK(second_generation == first_generation + 1);
    CHECK(gateway_slot_mark_inflight(&pool, 5, second_generation) == 0);
    CHECK(gateway_slot_complete(&pool, 5, first_generation) == -ESTALE);
    CHECK(gateway_slot_complete(&pool, 5, second_generation) == 0);
    CHECK(gateway_slot_prepare(&pool, 6, 64, &second_generation) == 0);
    CHECK(gateway_slot_cancel_ready(&pool, 6, second_generation) == 0);
    CHECK(gateway_slot_cancel_ready(&pool, 6, second_generation) == -ESTALE);
    puts("GATEWAY_SLOT_LIFECYCLE_PASS stale_completion=blocked generation=2");
    return 0;
}

int main(void)
{
    if (test_wire_contract() != 0 || test_spsc_ring() != 0 ||
        test_slot_lifecycle() != 0)
        return 1;
    puts("DPDK_RDMA_GATEWAY_PHASE1_CONTRACT_PASS");
    return 0;
}
