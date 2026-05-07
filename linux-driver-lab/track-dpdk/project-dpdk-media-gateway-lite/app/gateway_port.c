/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_port.c - DPDK ethdev 端口发现与初始化
 *
 * 端口初始化流程（每端口）：
 *   1. rte_eth_dev_configure   → 配置 RX/TX 队列数
 *   2. rte_eth_rx_queue_setup  → 配置 RX 队列（关联 mempool）
 *   3. rte_eth_tx_queue_setup  → 配置 TX 队列
 *   4. rte_eth_dev_start       → 启动端口
 *   5. rte_eth_promiscuous_enable → 开启混杂模式
 */

#include <stdio.h>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_lcore.h>

#include "gateway_port.h"

/* 获取并打印指定端口的 MAC 地址 */
void gw_print_port_mac(uint16_t portid)
{
    struct rte_ether_addr mac;
    int ret = rte_eth_macaddr_get(portid, &mac);
    if (ret == 0) {
        printf("port %u MAC: " RTE_ETHER_ADDR_PRT_FMT "\n",
               portid, RTE_ETHER_ADDR_BYTES(&mac));
    } else {
        printf("port %u MAC: unavailable ret=%d\n", portid, ret);
    }
}

/*------------------------------------------------------------------------------
 * gw_discover_ports - 发现系统中所有 DPDK 端口
 *
 * 使用 RTE_ETH_FOREACH_DEV 遍历所有 ethdev 设备
 * 支持 vdev (net_null, net_tap 等) 和物理网卡
 *----------------------------------------------------------------------------*/
int gw_discover_ports(uint16_t *ports, uint16_t max_ports, uint16_t *nb_ports)
{
    uint16_t portid;
    uint16_t count = 0;

    RTE_ETH_FOREACH_DEV(portid) {
        if (count >= max_ports)
            break;
        ports[count++] = portid;
    }

    *nb_ports = count;
    printf("available/initialized ports: %u\n", count);
    for (uint16_t i = 0; i < count; i++)
        gw_print_port_mac(ports[i]);

    return count > 0 ? 0 : -1;
}

/*------------------------------------------------------------------------------
 * gw_init_port - 初始化单个 DPDK 端口
 *
 * 初始化步骤：
 *   1. 获取 NUMA socket（优先使用网卡所在 socket）
 *   2. 配置 ethdev（1 个 RX 队列 + 1 个 TX 队列）
 *   3. 调整 descriptor 数量（驱动可能调整）
 *   4. 设置 RX 队列（关联 mbuf mempool）
 *   5. 设置 TX 队列
 *   6. 启动端口
 *   7. 开启混杂模式（可选）
 *----------------------------------------------------------------------------*/
int gw_init_port(uint16_t portid, struct rte_mempool *pool, const struct gw_config *cfg)
{
    struct rte_eth_conf port_conf = {0};
    uint16_t rx_desc = cfg->rx_desc;
    uint16_t tx_desc = cfg->tx_desc;
    int socket_id = rte_eth_dev_socket_id(portid);
    int ret;

    if (socket_id < 0)
        socket_id = rte_socket_id();

    /* 配置 1 个 RX 队列 + 1 个 TX 队列 */
    ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
    if (ret < 0) {
        fprintf(stderr, "rte_eth_dev_configure(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 驱动可能会调整 descriptor 数量，获取实际值 */
    ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &rx_desc, &tx_desc);
    if (ret < 0) {
        fprintf(stderr, "rte_eth_dev_adjust_nb_rx_tx_desc(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 设置 RX 队列：queue_id=0, socket_id=NUMA, NULL=rx_conf(默认) */
    ret = rte_eth_rx_queue_setup(portid, 0, rx_desc, socket_id, NULL, pool);
    if (ret < 0) {
        fprintf(stderr, "rte_eth_rx_queue_setup(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 设置 TX 队列 */
    ret = rte_eth_tx_queue_setup(portid, 0, tx_desc, socket_id, NULL);
    if (ret < 0) {
        fprintf(stderr, "rte_eth_tx_queue_setup(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 启动端口 */
    ret = rte_eth_dev_start(portid);
    if (ret < 0) {
        fprintf(stderr, "rte_eth_dev_start(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 混杂模式：接收所有 MAC 的包（用于网关/交换机） */
    if (cfg->promisc)
        rte_eth_promiscuous_enable(portid);

    printf("port %u started\n", portid);
    gw_print_port_mac(portid);
    return 0;
}

/* 停止并关闭所有端口（退出时清理） */
void gw_stop_ports(const uint16_t *ports, uint16_t nb_ports)
{
    for (uint16_t i = 0; i < nb_ports; i++) {
        uint16_t portid = ports[i];
        printf("stopping port %u\n", portid);
        rte_eth_dev_stop(portid);
        rte_eth_dev_close(portid);
    }
}
