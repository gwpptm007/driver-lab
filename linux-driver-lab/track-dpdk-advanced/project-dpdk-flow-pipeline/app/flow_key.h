#ifndef FLOW_KEY_H
#define FLOW_KEY_H

#include <stdint.h>
#include <rte_mbuf.h>

/* 固定 16 字节 key，避免结构体尾部未初始化字节影响 rte_hash。 */
struct flow_key {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol;
    uint8_t reserved[3];
};

int flow_key_extract(const struct rte_mbuf *mbuf, struct flow_key *key);
void flow_key_set_ipv4_udp(struct flow_key *key,
                           uint8_t src_a, uint8_t src_b,
                           uint8_t src_c, uint8_t src_d,
                           uint8_t dst_a, uint8_t dst_b,
                           uint8_t dst_c, uint8_t dst_d,
                           uint16_t src_port, uint16_t dst_port);

#endif
