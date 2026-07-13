#include "flow_capability.h"

#include <stdio.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_ethdev.h>
#include <rte_flow.h>

void flow_capability_probe(uint16_t port_id)
{
    struct rte_eth_dev_info info;
    struct rte_flow_attr attr;
    struct rte_flow_item pattern[4];
    struct rte_flow_action actions[2];
    struct rte_flow_item_ipv4 ipv4_spec;
    struct rte_flow_item_ipv4 ipv4_mask;
    struct rte_flow_item_udp udp_spec;
    struct rte_flow_item_udp udp_mask;
    struct rte_flow_error error;
    int ret;

    /* dev_info 描述队列/RSS 上限，validate 验证 pattern/action 是否可下沉。 */
    memset(&info, 0, sizeof(info));
    ret = rte_eth_dev_info_get(port_id, &info);
    if (ret == 0) {
        printf("FLOW_PORT_CAPABILITY max_rx_queues=%u max_tx_queues=%u"
               " reta_size=%u rss_offloads=0x%llx\n",
               info.max_rx_queues, info.max_tx_queues, info.reta_size,
               (unsigned long long)info.flow_type_rss_offloads);
        if (info.max_rx_queues >= 2 && info.reta_size > 0 &&
            info.flow_type_rss_offloads != 0)
            puts("RSS_MULTI_QUEUE_CAPABLE");
        else
            puts("RSS_MULTI_QUEUE_BOUNDARY_BLOCKED");
    }

    memset(&attr, 0, sizeof(attr));
    memset(pattern, 0, sizeof(pattern));
    memset(actions, 0, sizeof(actions));
    memset(&ipv4_spec, 0, sizeof(ipv4_spec));
    memset(&ipv4_mask, 0, sizeof(ipv4_mask));
    memset(&udp_spec, 0, sizeof(udp_spec));
    memset(&udp_mask, 0, sizeof(udp_mask));
    memset(&error, 0, sizeof(error));

    attr.ingress = 1;
    ipv4_spec.hdr.dst_addr = rte_cpu_to_be_32(RTE_IPV4(10, 20, 0, 1));
    ipv4_mask.hdr.dst_addr = UINT32_MAX;
    udp_spec.hdr.dst_port = rte_cpu_to_be_16(20001);
    udp_mask.hdr.dst_port = UINT16_MAX;
    pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
    pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[1].spec = &ipv4_spec;
    pattern[1].mask = &ipv4_mask;
    pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
    pattern[2].spec = &udp_spec;
    pattern[2].mask = &udp_mask;
    pattern[3].type = RTE_FLOW_ITEM_TYPE_END;
    actions[0].type = RTE_FLOW_ACTION_TYPE_DROP;
    actions[1].type = RTE_FLOW_ACTION_TYPE_END;

    /* 只做 validate，不创建规则，避免 capability probe 改变软件测试流量。 */
    ret = rte_flow_validate(port_id, &attr, pattern, actions, &error);
    if (ret == 0) {
        puts("RTE_FLOW_VALIDATE_PASS");
    } else {
        printf("RTE_FLOW_BOUNDARY_BLOCKED ret=%d type=%d message=%s\n",
               ret, error.type,
               error.message == NULL ? "unsupported" : error.message);
    }
}
