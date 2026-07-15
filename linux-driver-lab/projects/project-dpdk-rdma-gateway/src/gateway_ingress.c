#include "gateway_ingress.h"

#include <errno.h>
#include <netinet/in.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

static uint64_t flow_hash(const struct rte_ipv4_hdr *ip,
                          const struct rte_udp_hdr *udp)
{
    const uint8_t *fields[] = {
        (const uint8_t *)&ip->src_addr,
        (const uint8_t *)&ip->dst_addr,
        (const uint8_t *)&udp->src_port,
        (const uint8_t *)&udp->dst_port,
        &ip->next_proto_id,
    };
    const size_t lengths[] = {4, 4, 2, 2, 1};
    uint64_t hash = 1469598103934665603ULL;
    size_t field;

    /* FNV-1a 只用于稳定分流标识，不作为安全哈希。 */
    for (field = 0; field < sizeof(fields) / sizeof(fields[0]); ++field) {
        size_t index;

        for (index = 0; index < lengths[field]; ++index) {
            hash ^= fields[field][index];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

void gateway_ingress_init(struct gateway_ingress_context *context)
{
    memset(context, 0, sizeof(*context));
    gateway_slot_pool_init(&context->slot_pool);
    gateway_ring_init(&context->request_ring);
    context->next_request_id = 1;
}

int gateway_ingress_process(struct gateway_ingress_context *context,
                            const struct rte_mbuf *mbuf, uint16_t ingress_port,
                            uint16_t rx_queue)
{
    struct rte_ether_hdr eth_copy;
    struct rte_ipv4_hdr ip_copy;
    struct rte_udp_hdr udp_copy;
    const struct rte_ether_hdr *eth;
    const struct rte_ipv4_hdr *ip;
    const struct rte_udp_hdr *udp;
    const void *payload;
    struct gateway_request request;
    uint32_t ip_offset = sizeof(*eth);
    uint32_t payload_offset;
    uint32_t slot_id;
    uint32_t generation;
    uint16_t ip_header_len;
    uint16_t udp_len;
    uint16_t payload_len;
    int ret;

    context->stats.rx_packets++;
    eth = rte_pktmbuf_read(mbuf, 0, sizeof(*eth), &eth_copy);
    if (eth == NULL) {
        context->stats.malformed_packets++;
        return 0;
    }
    if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
        context->stats.unsupported_packets++;
        return 0;
    }

    ip = rte_pktmbuf_read(mbuf, ip_offset, sizeof(*ip), &ip_copy);
    if (ip == NULL || (ip->version_ihl >> 4) != 4) {
        context->stats.malformed_packets++;
        return 0;
    }
    ip_header_len = (uint16_t)((ip->version_ihl & 0x0fU) * 4U);
    if (ip_header_len < sizeof(*ip)) {
        context->stats.malformed_packets++;
        return 0;
    }
    if (ip->next_proto_id != IPPROTO_UDP) {
        context->stats.unsupported_packets++;
        return 0;
    }

    udp = rte_pktmbuf_read(mbuf, ip_offset + ip_header_len,
                           sizeof(*udp), &udp_copy);
    if (udp == NULL) {
        context->stats.malformed_packets++;
        return 0;
    }
    udp_len = rte_be_to_cpu_16(udp->dgram_len);
    if (udp_len <= sizeof(*udp)) {
        context->stats.malformed_packets++;
        return 0;
    }
    payload_len = (uint16_t)(udp_len - sizeof(*udp));
    if (payload_len > GATEWAY_MAX_PAYLOAD) {
        context->stats.malformed_packets++;
        return 0;
    }
    context->stats.udp_packets++;

    ret = gateway_slot_prepare_next(&context->slot_pool,
                                    &context->slot_cursor, payload_len,
                                    &slot_id, &generation);
    if (ret == -ENOSPC) {
        context->stats.slot_exhausted++;
        return 0;
    }
    if (ret != 0)
        return ret;

    payload_offset = ip_offset + ip_header_len + sizeof(*udp);
    payload = rte_pktmbuf_read(mbuf, payload_offset, payload_len,
                               context->staging[slot_id].payload);
    if (payload == NULL) {
        gateway_slot_cancel_ready(&context->slot_pool, slot_id, generation);
        context->stats.malformed_packets++;
        return 0;
    }
    if (payload != context->staging[slot_id].payload)
        memcpy(context->staging[slot_id].payload, payload, payload_len);

    memset(&request, 0, sizeof(request));
    request.request_id = context->next_request_id++;
    request.flow_hash = flow_hash(ip, udp);
    request.slot_id = slot_id;
    request.generation = generation;
    request.payload_len = payload_len;
    request.ingress_port = ingress_port;
    request.rx_queue = rx_queue;
    request.opcode = GATEWAY_OP_RDMA_WRITE;

    ret = gateway_ring_enqueue(&context->request_ring, &request);
    if (ret == -ENOSPC) {
        gateway_slot_cancel_ready(&context->slot_pool, slot_id, generation);
        context->stats.ring_full++;
        return 0;
    }
    if (ret != 0)
        return ret;
    context->stats.staged_requests++;
    return 0;
}

int gateway_mock_rdma_drain(struct gateway_ingress_context *context,
                            struct gateway_backend_stats *stats)
{
    struct gateway_request request;

    memset(stats, 0, sizeof(*stats));
    while (gateway_ring_dequeue(&context->request_ring, &request) == 0) {
        struct gateway_request decoded;
        uint8_t wire[GATEWAY_WIRE_HEADER_SIZE];

        stats->dequeued_requests++;
        if (gateway_slot_mark_inflight(&context->slot_pool, request.slot_id,
                                       request.generation) != 0 ||
            gateway_wire_encode(&request, wire, sizeof(wire)) != 0 ||
            gateway_wire_decode(wire, sizeof(wire), &decoded) != 0 ||
            decoded.request_id != request.request_id ||
            decoded.payload_len != request.payload_len ||
            memcmp(context->staging[request.slot_id].payload,
                   "GATEWAY_", 8) != 0)
            return -EINVAL;

        stats->payload_bytes += request.payload_len;
        if (gateway_slot_complete(&context->slot_pool, request.slot_id,
                                  request.generation) != 0)
            return -ESTALE;
        stats->completed_requests++;
    }
    return 0;
}
