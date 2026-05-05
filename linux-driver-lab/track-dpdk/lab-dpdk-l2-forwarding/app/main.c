/* SPDX-License-Identifier: BSD-3-Clause
 *
 * l2fwd-lite - a small educational DPDK L2 forwarding program.
 *
 * This lab is intentionally smaller than the upstream DPDK examples/l2fwd:
 *   - configure all available Ethernet ports with 1 RX queue and 1 TX queue
 *   - pair ports as 0<->1, 2<->3, ...
 *   - swap Ethernet source/destination addresses before forwarding
 *   - keep software counters and print periodic stats
 *   - if only one port exists, run a safe smoke path: RX and free packets
 *
 * It is designed for the current VMware test machine first, where ens192 is a
 * VMXNET3 device at PCI 0000:0b:00.0 and can be bound to uio_pci_generic.
 */

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

/* ========== DPDK 默认参数 ========== */
#define DEFAULT_NB_MBUF       8192U   /* mbuf 内存池中缓冲区数量 */
#define DEFAULT_MBUF_CACHE    250U    /* 每 lcore 的 mbuf 缓存数量 */
#define DEFAULT_BURST_SIZE    32U     /* 每次 RX/TX burst 收发包数量 */
#define DEFAULT_RX_DESC       1024U   /* RX 队列描述符数量 */
#define DEFAULT_TX_DESC       1024U   /* TX 队列描述符数量 */
#define DEFAULT_STATS_PERIOD  2U      /* 统计打印间隔（秒） */
#define MAX_BURST_SIZE        128U    /* 最大 burst 大小 */

/* ========== 应用配置结构 ========== */
struct app_config {
    uint32_t nb_mbuf;        /* mbuf 池大小 */
    uint32_t mbuf_cache;     /* per-lcore 缓存 */
    uint16_t burst_size;     /* 轮询 burst 大小 */
    uint16_t rx_desc;        /* RX 队列深度 */
    uint16_t tx_desc;        /* TX 队列深度 */
    uint32_t run_seconds;    /* 运行时间，0=无限 */
    uint32_t stats_period;   /* 统计打印间隔 */
    bool promisc;            /* 混杂模式开关 */
};

/* ========== 软件统计结构 ========== */
struct port_sw_stats {
    uint64_t rx_packets;     /* 收到的包数 */
    uint64_t rx_bytes;       /* 收到的字节数 */
    uint64_t tx_packets;     /* 发送的包数 */
    uint64_t tx_bytes;       /* 发送的字节数 */
    uint64_t tx_failed;      /* 发送失败（TX 队列满） */
    uint64_t no_peer_drop;   /* 无配对端口而丢弃的包数 */
};

static volatile bool force_quit;  /* SIGINT/SIGTERM 退出标志 */

/* ========== DPDK 全局状态 ========== */
static struct app_config cfg = {
    .nb_mbuf = DEFAULT_NB_MBUF,
    .mbuf_cache = DEFAULT_MBUF_CACHE,
    .burst_size = DEFAULT_BURST_SIZE,
    .rx_desc = DEFAULT_RX_DESC,
    .tx_desc = DEFAULT_TX_DESC,
    .run_seconds = 15,
    .stats_period = DEFAULT_STATS_PERIOD,
    .promisc = true,
};

/* DPDK 端口 ID 数组，记录已初始化的端口 */
static uint16_t port_ids[RTE_MAX_ETHPORTS];
static uint16_t nb_ports_used;  /* 已使用端口数量 */
static struct port_sw_stats sw_stats[RTE_MAX_ETHPORTS];

/* ========== 信号处理 ========== */
static void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        force_quit = true;
    }
}

/* ========== 命令行帮助 ========== */
static void usage(const char *prog)
{
    printf("Usage: %s [EAL options] -- [APP options]\n", prog);
    printf("\nAPP options:\n");
    printf("  --run-seconds N       run duration, 0 means until Ctrl-C (default: %u)\n", cfg.run_seconds);
    printf("  --stats-period N      periodic stats interval in seconds (default: %u)\n", cfg.stats_period);
    printf("  --burst-size N        RX/TX burst size, max %u (default: %u)\n", MAX_BURST_SIZE, cfg.burst_size);
    printf("  --nb-mbuf N           mbuf count in pool (default: %u)\n", cfg.nb_mbuf);
    printf("  --rx-desc N           RX descriptors per port (default: %u)\n", cfg.rx_desc);
    printf("  --tx-desc N           TX descriptors per port (default: %u)\n", cfg.tx_desc);
    printf("  --promisc 0|1         disable/enable promiscuous mode (default: %u)\n", cfg.promisc ? 1U : 0U);
    printf("  --help                show this help\n");
}

/* ========== 解析无符号整数参数 ========== */
static int parse_u32(const char *name, const char *value, uint32_t *out)
{
    char *end = NULL;
    unsigned long v;

    if (value == NULL || value[0] == '\0') {
        fprintf(stderr, "missing value for %s\n", name);
        return -1;
    }

    errno = 0;
    v = strtoul(value, &end, 0);
    if (errno != 0 || end == value || *end != '\0' || v > UINT32_MAX) {
        fprintf(stderr, "invalid integer for %s: %s\n", name, value);
        return -1;
    }

    *out = (uint32_t)v;
    return 0;
}

/* ========== 解析应用层参数 ========== */
static int parse_app_args(int argc, char **argv)
{
    int i;

    for (i = 0; i < argc; i++) {
        const char *arg = argv[i];
        uint32_t v;

        if (arg == NULL) {
            continue;
        }

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            usage("l2fwd-lite");
            return 1;
        } else if (strcmp(arg, "--run-seconds") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &cfg.run_seconds) < 0)
                return -1;
        } else if (strcmp(arg, "--stats-period") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &cfg.stats_period) < 0)
                return -1;
        } else if (strcmp(arg, "--burst-size") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &v) < 0)
                return -1;
            if (v == 0 || v > MAX_BURST_SIZE) {
                fprintf(stderr, "--burst-size must be 1..%u\n", MAX_BURST_SIZE);
                return -1;
            }
            cfg.burst_size = (uint16_t)v;
        } else if (strcmp(arg, "--nb-mbuf") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &cfg.nb_mbuf) < 0)
                return -1;
        } else if (strcmp(arg, "--rx-desc") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &v) < 0)
                return -1;
            cfg.rx_desc = (uint16_t)v;
        } else if (strcmp(arg, "--tx-desc") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &v) < 0)
                return -1;
            cfg.tx_desc = (uint16_t)v;
        } else if (strcmp(arg, "--promisc") == 0) {
            if (++i >= argc || parse_u32(arg, argv[i], &v) < 0)
                return -1;
            if (v > 1) {
                fprintf(stderr, "--promisc must be 0 or 1\n");
                return -1;
            }
            cfg.promisc = (v == 1);
        } else if (strncmp(arg, "--", 2) == 0) {
            fprintf(stderr, "unknown APP option: %s\n", arg);
            usage("l2fwd-lite");
            return -1;
        }
    }

    if (cfg.stats_period == 0) {
        cfg.stats_period = DEFAULT_STATS_PERIOD;
    }

    return 0;
}

/* ========== 打印 MAC 地址 ========== */
static void print_mac(uint16_t portid)
{
    struct rte_ether_addr mac;
    /* 获取端口的 MAC 地址 */
    int ret = rte_eth_macaddr_get(portid, &mac);

    if (ret == 0) {
        printf("port %u MAC: " RTE_ETHER_ADDR_PRT_FMT "\n",
               portid, RTE_ETHER_ADDR_BYTES(&mac));
    } else {
        printf("port %u MAC: unavailable, ret=%d\n", portid, ret);
    }
}

/* ========== 获取配对端口 ==========
 *
 * DPDK 端口配对规则：按初始化顺序两两配对
 *   索引 0 <-> 1,  索引 2 <-> 3,  索引 4 <-> 5, ...
 */
static uint16_t paired_port(uint16_t portid)
{
    uint16_t i;

    for (i = 0; i < nb_ports_used; i++) {
        if (port_ids[i] != portid) {
            continue;
        }

        if ((i % 2) == 0) {
            /* 偶数索引：配对到下一个 */
            if (i + 1 < nb_ports_used) {
                return port_ids[i + 1];
            }
        } else {
            /* 奇数索引：配对到上一个 */
            return port_ids[i - 1];
        }
    }

    return RTE_MAX_ETHPORTS;  /* 无配对端口 */
}

/* ========== MAC 地址交换 ==========
 *
 * L2 转发时交换 src/dst MAC，模拟交换机行为
 * 原始:   dst=MAC_B  src=MAC_A
 * 交换后: dst=MAC_A  src=MAC_B
 */
static void maybe_swap_eth_addr(struct rte_mbuf *m)
{
    struct rte_ether_hdr *eth;
    struct rte_ether_addr tmp;

    if (rte_pktmbuf_data_len(m) < sizeof(*eth)) {
        return;
    }

    eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    tmp = eth->src_addr;
    eth->src_addr = eth->dst_addr;
    eth->dst_addr = tmp;
}

/* ========== 打印软件统计 ========== */
static void print_sw_stats(void)
{
    uint16_t i;

    printf("\n==== l2fwd-lite software stats ====\n");
    for (i = 0; i < nb_ports_used; i++) {
        const uint16_t portid = port_ids[i];
        const struct port_sw_stats *s = &sw_stats[portid];

        printf("port %u: rx=%" PRIu64 " rx_bytes=%" PRIu64
               " tx=%" PRIu64 " tx_bytes=%" PRIu64
               " tx_failed=%" PRIu64 " no_peer_drop=%" PRIu64 "\n",
               portid,
               s->rx_packets, s->rx_bytes,
               s->tx_packets, s->tx_bytes,
               s->tx_failed, s->no_peer_drop);
    }
    printf("===================================\n");
}

/* ========== 打印网卡硬件统计 ==========
 *
 * rte_eth_stats_get 获取网卡驱动层统计
 */
static void print_ethdev_stats(void)
{
    uint16_t i;

    printf("\n==== rte_eth_stats ====\n");
    for (i = 0; i < nb_ports_used; i++) {
        const uint16_t portid = port_ids[i];
        struct rte_eth_stats stats;
        int ret = rte_eth_stats_get(portid, &stats);

        if (ret != 0) {
            printf("port %u: rte_eth_stats_get failed: %d\n", portid, ret);
            continue;
        }

        printf("port %u: ipackets=%" PRIu64 " opackets=%" PRIu64
               " ibytes=%" PRIu64 " obytes=%" PRIu64
               " imissed=%" PRIu64 " ierrors=%" PRIu64 " oerrors=%" PRIu64 "\n",
               portid,
               stats.ipackets, stats.opackets,
               stats.ibytes, stats.obytes,
               stats.imissed, stats.ierrors, stats.oerrors);
    }
    printf("=======================\n");
}

/* ========== 初始化单个端口 ==========
 *
 * DPDK 端口初始化流程：
 *   1. rte_eth_dev_info_get     → 获取设备能力
 *   2. rte_eth_dev_configure     → 配置端口（队列数等）
 *   3. rte_eth_rx_queue_setup    → 设置 RX 队列
 *   4. rte_eth_tx_queue_setup    → 设置 TX 队列
 *   5. rte_eth_dev_start         → 启动端口
 */
static int init_port(uint16_t portid, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;
    uint16_t rx_desc = cfg.rx_desc;
    uint16_t tx_desc = cfg.tx_desc;
    int socket_id;
    int ret;

    memset(&port_conf, 0, sizeof(port_conf));
    memset(&dev_info, 0, sizeof(dev_info));

    /* 获取设备信息（驱动名、队列能力等）*/
    ret = rte_eth_dev_info_get(portid, &dev_info);
    if (ret != 0) {
        printf("rte_eth_dev_info_get(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 启用 TX offload: MBUF_FAST_FREE 加速 */
    if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
        port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
    }

    /* 配置端口：1 个 RX 队列，1 个 TX 队列 */
    ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
    if (ret < 0) {
        printf("rte_eth_dev_configure(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 调整描述符数量（网卡可能调整为我们请求的值）*/
    ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &rx_desc, &tx_desc);
    if (ret < 0) {
        printf("rte_eth_dev_adjust_nb_rx_tx_desc(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 获取端口所在 NUMA socket */
    socket_id = rte_eth_dev_socket_id(portid);
    if (socket_id < 0) {
        socket_id = rte_socket_id();
    }

    /* 设置 RX 队列：分配 mbuf_pool 中的缓冲区 */
    ret = rte_eth_rx_queue_setup(portid, 0, rx_desc, (unsigned int)socket_id, NULL, mbuf_pool);
    if (ret < 0) {
        printf("rte_eth_rx_queue_setup(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 设置 TX 队列 */
    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    ret = rte_eth_tx_queue_setup(portid, 0, tx_desc, (unsigned int)socket_id, &txconf);
    if (ret < 0) {
        printf("rte_eth_tx_queue_setup(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 启动端口，开始收发包 */
    ret = rte_eth_dev_start(portid);
    if (ret < 0) {
        printf("rte_eth_dev_start(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    /* 混杂模式：接收所有 MAC 的包 */
    if (cfg.promisc) {
        ret = rte_eth_promiscuous_enable(portid);
        if (ret != 0) {
            printf("warning: rte_eth_promiscuous_enable(port=%u) failed: %d\n", portid, ret);
        }
    }

    printf("port %u started: rx_desc=%u tx_desc=%u socket=%d driver=%s\n",
           portid, rx_desc, tx_desc, socket_id,
           dev_info.driver_name ? dev_info.driver_name : "unknown");
    print_mac(portid);

    return 0;
}

/* ========== 初始化所有可用端口 ==========
 *
 * RTE_ETH_FOREACH_DEV 遍历所有 DPDK 网卡
 */
static int init_all_ports(struct rte_mempool *mbuf_pool)
{
    uint16_t portid;
    int ret;

    RTE_ETH_FOREACH_DEV(portid) {
        if (nb_ports_used >= RTE_MAX_ETHPORTS) {
            break;
        }

        printf("initializing port %u\n", portid);
        ret = init_port(portid, mbuf_pool);
        if (ret != 0) {
            return ret;
        }

        port_ids[nb_ports_used++] = portid;
    }

    if (nb_ports_used == 0) {
        fprintf(stderr, "no available DPDK ethdev ports found\n");
        return -ENODEV;
    }

    printf("available/initialized ports: %u\n", nb_ports_used);
    if (nb_ports_used < 2) {
        printf("notice: only one port is available; running RX/free smoke mode, no L2 peer forwarding.\n");
    } else if ((nb_ports_used % 2) != 0) {
        printf("notice: odd number of ports; the last port will RX/free because it has no peer.\n");
    }

    return 0;
}

/* ========== 转发主循环 ==========
 *
 * 轮询模式：持续调用 rte_eth_rx_burst / rte_eth_tx_burst
 *
 * 流程：
 *   for 每个端口:
 *       rte_eth_rx_burst()   → 从网卡接收包到 mbuf
 *       交换 MAC 地址
 *       rte_eth_tx_burst()   → 发送到配对端口
 *       释放无法发送的 mbuf
 */
static void forwarding_loop(void)
{
    const uint64_t hz = rte_get_timer_hz();  /* CPU 频率，用于定时 */
    const uint64_t start_tsc = rte_get_timer_cycles();
    uint64_t next_stats_tsc = start_tsc + (uint64_t)cfg.stats_period * hz;
    struct rte_mbuf *pkts[MAX_BURST_SIZE];

    printf("enter forwarding loop: run_seconds=%u stats_period=%u burst=%u lcore=%u\n",
           cfg.run_seconds, cfg.stats_period, cfg.burst_size, rte_lcore_id());

    while (!force_quit) {
        uint16_t i;
        const uint64_t now = rte_get_timer_cycles();

        /* 检查运行时间 */
        if (cfg.run_seconds > 0 && now - start_tsc >= (uint64_t)cfg.run_seconds * hz) {
            printf("run_seconds reached, stopping...\n");
            break;
        }

        for (i = 0; i < nb_ports_used; i++) {
            const uint16_t src = port_ids[i];
            const uint16_t dst = paired_port(src);
            uint16_t nb_rx;
            uint16_t j;

            /* 从 src 端口接收包（轮询）*/
            nb_rx = rte_eth_rx_burst(src, 0, pkts, cfg.burst_size);
            if (nb_rx == 0) {
                continue;
            }

            sw_stats[src].rx_packets += nb_rx;
            for (j = 0; j < nb_rx; j++) {
                sw_stats[src].rx_bytes += rte_pktmbuf_pkt_len(pkts[j]);
                maybe_swap_eth_addr(pkts[j]);  /* L2 MAC 交换 */
            }

            /* 无配对端口：直接释放 mbuf */
            if (dst == RTE_MAX_ETHPORTS) {
                for (j = 0; j < nb_rx; j++) {
                    rte_pktmbuf_free(pkts[j]);
                }
                sw_stats[src].no_peer_drop += nb_rx;
                continue;
            }

            /* 发送到配对端口 dst */
            const uint16_t nb_tx = rte_eth_tx_burst(dst, 0, pkts, nb_rx);
            sw_stats[dst].tx_packets += nb_tx;
            for (j = 0; j < nb_tx; j++) {
                sw_stats[dst].tx_bytes += rte_pktmbuf_pkt_len(pkts[j]);
            }

            /* TX 队列满导致发送失败：释放剩余 mbuf */
            if (nb_tx < nb_rx) {
                sw_stats[dst].tx_failed += nb_rx - nb_tx;
                for (j = nb_tx; j < nb_rx; j++) {
                    rte_pktmbuf_free(pkts[j]);
                }
            }
        }

        /* 定期打印统计 */
        if (now >= next_stats_tsc) {
            print_sw_stats();
            next_stats_tsc = now + (uint64_t)cfg.stats_period * hz;
        }
    }
}

/* ========== 停止所有端口 ========== */
static void stop_all_ports(void)
{
    uint16_t i;

    for (i = 0; i < nb_ports_used; i++) {
        const uint16_t portid = port_ids[i];
        printf("stopping port %u\n", portid);
        rte_eth_dev_stop(portid);
        rte_eth_dev_close(portid);
    }
}

/* ========== 主函数 ==========
 *
 * DPDK 应用入口顺序：
 *   1. rte_eal_init()        初始化 DPDK 环境
 *   2. parse_app_args()       解析应用参数
 *   3. 创建 mbuf pool          rte_pktmbuf_pool_create
 *   4. init_all_ports()        初始化所有端口
 *   5. forwarding_loop()       进入转发循环
 *   6. print stats             打印统计
 *   7. stop_all_ports()        停止端口
 *   8. rte_eal_cleanup()      清理 DPDK
 */
int main(int argc, char **argv)
{
    struct rte_mempool *mbuf_pool;
    int ret;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* ========== 1. EAL 初始化 ==========
     *
     * rte_eal_init 是 DPDK 程序的第一个调用
     * 解析 -l (lcore), -n (内存通道), --file-prefix, -a (PCI) 等参数
     * 初始化 hugepages、内存池、多进程通信等
     */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eal_init failed\n");
    }

    /* rte_eal_init 会消耗它认识的参数，返回剩余未处理的参数 */
    argc -= ret;
    argv += ret;

    /* 解析应用参数（--run-seconds, --stats-period 等）*/
    ret = parse_app_args(argc, argv);
    if (ret > 0) {
        rte_eal_cleanup();
        return 0;
    }
    if (ret < 0) {
        rte_eal_cleanup();
        return 1;
    }

    printf("l2fwd-lite config: nb_mbuf=%u mbuf_cache=%u rx_desc=%u tx_desc=%u burst=%u promisc=%u\n",
           cfg.nb_mbuf, cfg.mbuf_cache, cfg.rx_desc, cfg.tx_desc, cfg.burst_size, cfg.promisc ? 1U : 0U);

    /* ========== 2. 创建 mbuf 内存池 ==========
     *
     * rte_pktmbuf_pool_create 在 hugepages 上分配 mbuf 池
     * 每个 mbuf 默认 2176 字节（headroom + 2KB MTU）
     * mbuf 池供 RX/TX 队列使用
     */
    mbuf_pool = rte_pktmbuf_pool_create("l2fwd_lite_mbuf_pool",
                                        cfg.nb_mbuf,
                                        cfg.mbuf_cache,
                                        0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "rte_pktmbuf_pool_create failed\n");
    }

    /* ========== 3. 初始化所有端口 ========== */
    ret = init_all_ports(mbuf_pool);
    if (ret != 0) {
        rte_eal_cleanup();
        return 1;
    }

    /* ========== 4. 进入转发主循环 ========== */
    forwarding_loop();

    /* ========== 5. 打印统计 ========== */
    print_sw_stats();
    print_ethdev_stats();

    /* ========== 6. 停止端口 ========== */
    stop_all_ports();

    /* ========== 7. 清理 EAL ========== */
    ret = rte_eal_cleanup();
    if (ret != 0) {
        printf("rte_eal_cleanup returned %d\n", ret);
    }

    printf("bye\n");
    return 0;
}
