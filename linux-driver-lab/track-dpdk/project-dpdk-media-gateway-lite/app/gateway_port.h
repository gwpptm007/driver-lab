/* SPDX-License-Identifier: BSD-3-Clause
 *
 * gateway_port.h - DPDK ethdev 端口发现与初始化
 *
 * 端口初始化流程：
 *   gw_discover_ports → 枚举系统中所有 ethdev 端口
 *   gw_init_port      → 初始化单个端口（RX/TX 队列、mempool、启动）
 *   gw_stop_ports     → 停止并关闭所有端口
 */

#ifndef GATEWAY_PORT_H
#define GATEWAY_PORT_H

#include <stdint.h>

#include <rte_mempool.h>

#include "gateway_config.h"
#include "gateway_common.h"

/* 发现所有可用 DPDK 端口（ethdev 枚举） */
int gw_discover_ports(uint16_t *ports, uint16_t max_ports, uint16_t *nb_ports);

/* 初始化单个端口（配置队列、关联 mempool、启动） */
int gw_init_port(uint16_t portid, struct rte_mempool *pool, const struct gw_config *cfg);

/* 停止并关闭所有端口（退出清理） */
void gw_stop_ports(const uint16_t *ports, uint16_t nb_ports);

/* 打印端口 MAC 地址（调试用） */
void gw_print_port_mac(uint16_t portid);

#endif /* GATEWAY_PORT_H */
