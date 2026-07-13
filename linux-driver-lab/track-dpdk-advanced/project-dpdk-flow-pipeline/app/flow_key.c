#include "flow_key.h"

#include <errno.h>
#include <netinet/in.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

int flow_key_extract(const struct rte_mbuf *mbuf, struct flow_key *key)
{
    struct rte_ether_hdr eth_copy;
    struct rte_ipv4_hdr ip_copy;
    struct rte_udp_hdr udp_copy;
    const struct rte_ether_hdr *eth;
    const struct rte_ipv4_hdr *ip;
    const struct rte_udp_hdr *udp;
    uint32_t offset = 0;
    uint16_t ip_header_len;

    /* rte_pktmbuf_read 可处理跨 segment 的头部，并在越界时返回 NULL。 */
    memset(key, 0, sizeof(*key));
    eth = rte_pktmbuf_read(mbuf, offset, sizeof(*eth), &eth_copy);
    if (eth == NULL)
        return -EINVAL;
    if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
        return -ENOTSUP;

    offset += sizeof(*eth);
    ip = rte_pktmbuf_read(mbuf, offset, sizeof(*ip), &ip_copy);
    if (ip == NULL || (ip->version_ihl >> 4) != 4)
        return -EINVAL;
    ip_header_len = (uint16_t)((ip->version_ihl & 0x0f) * 4U);
    if (ip_header_len < sizeof(*ip) || ip->next_proto_id != IPPROTO_UDP)
        return -ENOTSUP;

    udp = rte_pktmbuf_read(mbuf, offset + ip_header_len,
                           sizeof(*udp), &udp_copy);
    if (udp == NULL)
        return -EINVAL;

    /* 地址和端口保持网络字节序，规则构造时使用相同表示。 */
    key->src_addr = ip->src_addr;
    key->dst_addr = ip->dst_addr;
    key->src_port = udp->src_port;
    key->dst_port = udp->dst_port;
    key->protocol = ip->next_proto_id;
    return 0;
}

void flow_key_set_ipv4_udp(struct flow_key *key,
                           uint8_t src_a, uint8_t src_b,
                           uint8_t src_c, uint8_t src_d,
                           uint8_t dst_a, uint8_t dst_b,
                           uint8_t dst_c, uint8_t dst_d,
                           uint16_t src_port, uint16_t dst_port)
{
    uint32_t src = ((uint32_t)src_a << 24) | ((uint32_t)src_b << 16) |
                   ((uint32_t)src_c << 8) | src_d;
    uint32_t dst = ((uint32_t)dst_a << 24) | ((uint32_t)dst_b << 16) |
                   ((uint32_t)dst_c << 8) | dst_d;

    memset(key, 0, sizeof(*key));
    key->src_addr = rte_cpu_to_be_32(src);
    key->dst_addr = rte_cpu_to_be_32(dst);
    key->src_port = rte_cpu_to_be_16(src_port);
    key->dst_port = rte_cpu_to_be_16(dst_port);
    key->protocol = IPPROTO_UDP;
}
