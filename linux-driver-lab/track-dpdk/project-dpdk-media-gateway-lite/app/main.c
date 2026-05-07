/* SPDX-License-Identifier: BSD-3-Clause
 *
 * media-gateway-lite - 简化版 DPDK 用户态媒体网关
 *
 * 设计原则：代码小而清晰，便于学习理解
 *   - 每端口一个 RX/TX 队列
 *   - 命令行静态配置规则表
 *   - Ethernet/ARP/IPv4/UDP 协议解析分类
 *   - UDP-only 媒体快路径
 *   - MAC/IP/UDP 头部改写
 *   - per-port 和 per-rule 软件统计
 */

#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#include "gateway_config.h"
#include "gateway_packet.h"
#include "gateway_port.h"
#include "gateway_stats.h"

/* 全局变量：配置、端口列表、运行时统计 */
static volatile bool force_quit;       /* 强制退出标志（Ctrl+C/SIGTERM） */
static struct gw_config cfg;           /* 网关配置（规则、策略等） */
static uint16_t ports[GW_MAX_PORTS];   /* 已发现的 DPDK 端口列表 */
static uint16_t nb_ports;              /* 端口数量 */
static struct gw_runtime_stats stats;   /* 软件统计计数器 */

/* 信号处理：捕获 SIGINT/SIGTERM 实现优雅退出 */
static void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        force_quit = true;
}

/* 检查目标端口是否处于活跃状态 */
static bool port_is_active(uint16_t portid)
{
    for (uint16_t i = 0; i < nb_ports; i++) {
        if (ports[i] == portid)
            return true;
    }
    return false;
}

/*------------------------------------------------------------------------------
 * forward_or_drop - 转发或丢弃报文
 *
 * 根据分类结果决定：
 *   - 匹配规则且目标端口活跃 → 发送（TX burst）
 *   - 无匹配或目标端口不活跃 → 丢弃（释放 mbuf）
 *----------------------------------------------------------------------------*/
static void forward_or_drop(uint16_t in_port, struct rte_mbuf *m, struct gw_packet_result r)
{
    /* 不转发或目标端口不存在 → 丢弃 */
    if (!r.forward || !port_is_active(r.out_port)) {
        stats.port[in_port].drops++;
        rte_pktmbuf_free(m);  /* 释放 mbuf 内存池 */
        return;
    }

    /* 尝试从目标端口发送 */
    struct rte_mbuf *tx_pkts[1] = { m };
    uint16_t sent = rte_eth_tx_burst(r.out_port, 0, tx_pkts, 1);
    if (sent == 1) {
        /* 发送成功：更新目标端口 TX 统计 */
        stats.port[r.out_port].tx++;
        stats.port[r.out_port].tx_bytes += rte_pktmbuf_pkt_len(m);
    } else {
        /* 发送失败（TX ring 满）：更新失败计数并丢弃 */
        stats.port[r.out_port].tx_failed++;
        stats.port[in_port].drops++;
        rte_pktmbuf_free(m);
    }
}

/*------------------------------------------------------------------------------
 * run_loop - 主循环（单 lcore poll mode）
 *
 * 工作流程：
 *   1. 按周期打印统计（可 Ctrl+C 中断）
 *   2. 遍历所有端口，调用 rte_eth_rx_burst 收包
 *   3. 每包调用 gw_packet_process 分类处理
 *   4. 根据结果 forward_or_drop
 *----------------------------------------------------------------------------*/
static void run_loop(void)
{
    struct rte_mbuf *pkts[GW_MAX_BURST_SIZE];
    const uint64_t hz = rte_get_timer_hz();
    const uint64_t start = rte_get_timer_cycles();
    uint64_t next_stats = start + (uint64_t)cfg.stats_period * hz;  /* 下次打印统计时间 */
    uint64_t end = 0;

    /* run_seconds=0 表示无限运行（直到 Ctrl+C） */
    if (cfg.run_seconds != 0)
        end = start + (uint64_t)cfg.run_seconds * hz;

    printf("enter media gateway loop on lcore %u\n", rte_lcore_id());

    /* DPDK poll mode 主循环 */
    while (!force_quit) {
        const uint64_t now = rte_get_timer_cycles();
        /* 超时退出（run_seconds 到达） */
        if (end != 0 && now >= end)
            break;

        /* 遍历所有已初始化端口 */
        for (uint16_t i = 0; i < nb_ports; i++) {
            const uint16_t portid = ports[i];

            /* RX burst：批量收取报文（最多 burst_size 个） */
            const uint16_t nb_rx = rte_eth_rx_burst(portid, 0, pkts, cfg.burst_size);
            if (nb_rx == 0)
                continue;

            /* 逐包处理 */
            for (uint16_t j = 0; j < nb_rx; j++) {
                struct rte_mbuf *m = pkts[j];

                /* 更新 RX 统计（端口级别） */
                stats.port[portid].rx++;
                stats.port[portid].rx_bytes += rte_pktmbuf_pkt_len(m);

                /* 解析报文 → 匹配规则 → 决定转发/丢弃 */
                struct gw_packet_result r = gw_packet_process(portid, m, &cfg, &stats);
                forward_or_drop(portid, m, r);
            }
        }

        /* 按周期打印统计（软件计数器 + ethdev 硬件计数器） */
        if (now >= next_stats) {
            gw_stats_print(&stats, ports, nb_ports, cfg.nb_rules);
            gw_ethdev_stats_print(ports, nb_ports);
            next_stats = now + (uint64_t)cfg.stats_period * hz;
        }
    }

    /* 退出前打印最终统计 */
    gw_stats_print(&stats, ports, nb_ports, cfg.nb_rules);
    gw_ethdev_stats_print(ports, nb_ports);
}

/*==============================================================================
 * main - 程序入口
 *
 * 初始化顺序：
 *   1. 解析配置默认值
 *   2. EAL 初始化（DPDK 环境抽象层）
 *   3. 解析命令行参数（EAL 选项 + APP 选项）
 *   4. 创建 mbuf mempool（报文缓冲池）
 *   5. 发现并初始化所有 DPDK 端口
 *   6. 运行主循环
 *   7. 清理退出
 *============================================================================*/
int main(int argc, char **argv)
{
    int ret;
    struct rte_mempool *pool;

    /* 初始化默认配置 */
    gw_config_init(&cfg);

    /* 注册信号处理（优雅退出） */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* EAL 初始化：DPDK 必须首先调用
     * argc/argv 会消耗 EAL 相关参数，剩余参数给 APP 使用 */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        fprintf(stderr, "rte_eal_init failed\n");
        return 1;
    }
    argc -= ret;
    argv += ret;

    /* 解析 APP 参数（规则、策略、超时等） */
    ret = gw_config_parse_args(&cfg, argc, argv);
    if (ret > 0)
        return 0;  /* --help 打印后正常退出 */
    if (ret < 0)
        return 1;

    printf("media-gateway-lite starting\n");

    /* 创建 mbuf 内存池：所有收发的报文都需要从池中分配
     * mbuf 是 DPDK 中报文缓冲的核心结构 */
    pool = rte_pktmbuf_pool_create("media_gateway_mbuf_pool",
                                   cfg.nb_mbuf,    /* mbuf 数量（默认 8192） */
                                   cfg.mbuf_cache,  /* per-lcore cache 大小 */
                                   0,               /* priv size（通常为 0） */
                                   RTE_MBUF_DEFAULT_BUF_SIZE,  /* 数据区大小 */
                                   rte_socket_id());  /* NUMA 节点 */
    if (pool == NULL) {
        fprintf(stderr, "rte_pktmbuf_pool_create failed\n");
        return 1;
    }

    /* 发现系统中可用的 DPDK 端口（ethdev 枚举） */
    if (gw_discover_ports(ports, GW_MAX_PORTS, &nb_ports) < 0) {
        fprintf(stderr, "no usable DPDK port found\n");
        return 1;
    }

    /* 如果没有命令行指定规则，自动生成双向默认规则
     * 例如：port0 → port1, port1 → port0 */
    gw_rules_prepare_defaults(&cfg, ports, nb_ports);
    gw_config_print(&cfg);  /* 打印最终配置供调试 */

    /* 初始化软件统计计数器 */
    gw_stats_reset(&stats);

    /* 初始化每个端口（配置 RX/TX 队列、启动设备） */
    for (uint16_t i = 0; i < nb_ports; i++) {
        ret = gw_init_port(ports[i], pool, &cfg);
        if (ret < 0) {
            gw_stop_ports(ports, i);  /* 失败时停止已初始化的端口 */
            return 1;
        }
    }

    /* 进入主循环（poll mode） */
    run_loop();

    printf("leaving media gateway loop\n");
    gw_stop_ports(ports, nb_ports);  /* 停止所有端口 */
    printf("bye\n");
    return 0;
}
