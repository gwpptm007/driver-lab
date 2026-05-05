/* SPDX-License-Identifier: BSD-3-Clause
 *
 * fastpath-lite - educational DPDK user-space fastpath prototype.
 *
 * This project is the capstone of track-dpdk:
 *   - starts from the l2fwd-lite lab baseline
 *   - adds packet classification for ARP / IPv4 / UDP
 *   - supports optional UDP-only filtering
 *   - supports optional L2/L3/L4 rewrite for UDP/IPv4 traffic
 *   - keeps explicit software counters for review records
 *
 * It is intentionally small and readable, not a replacement for production
 * DPDK dataplane code.  The goal is to make the user's previous media-plane
 * experience reproducible on the current VMware test machine.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_udp.h>

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
    bool udp_only;           /* UDP-only 过滤开关 */
    bool swap_mac;           /* MAC 交换开关 */
    bool rewrite_enable;     /* rewrite 规则开关 */

    /* rewrite 目标地址（网络字节序）*/
    bool set_src_mac;
    bool set_dst_mac;
    bool set_src_ip;
    bool set_dst_ip;
    bool set_src_port;
    bool set_dst_port;

    struct rte_ether_addr rewrite_src_mac;
    struct rte_ether_addr rewrite_dst_mac;
    uint32_t rewrite_src_ip;     /* network byte order */
    uint32_t rewrite_dst_ip;     /* network byte order */
    uint16_t rewrite_src_port;   /* network byte order */
    uint16_t rewrite_dst_port;   /* network byte order */
};

/* ========== 软件统计结构（比 l2fwd-lite 更丰富）============ */
struct port_sw_stats {
    uint64_t rx_packets;     /* 收到的包数 */
    uint64_t rx_bytes;       /* 收到的字节数 */
    uint64_t tx_packets;     /* 发送的包数 */
    uint64_t tx_bytes;       /* 发送的字节数 */
    uint64_t tx_failed;      /* 发送失败（TX 队列满）*/

    /* 分类统计 */
    uint64_t arp_packets;    /* ARP 包数 */
    uint64_t ipv4_packets;   /* IPv4 包数 */
    uint64_t udp_packets;   /* UDP 包数 */
    uint64_t non_udp_packets; /* 非 UDP 包数 */

    /* rewrite 统计 */
    uint64_t rewrite_packets; /* 被 rewrite 的包数 */
    uint64_t drop_short;     /* 包太短被丢弃 */
    uint64_t drop_non_udp;   /* udp_only 模式下非 UDP 被丢弃 */
    uint64_t drop_no_peer;    /* 无配对端口而丢弃 */
};

static volatile bool force_quit;  /* SIGINT/SIGTERM 退出标志 */

/* ========== DPDK 全局状态 ========== */
static struct app_config cfg = {
    .nb_mbuf = DEFAULT_NB_MBUF,
    .mbuf_cache = DEFAULT_MBUF_CACHE,
    .burst_size = DEFAULT_BURST_SIZE,
    .rx_desc = DEFAULT_RX_DESC,
    .tx_desc = DEFAULT_TX_DESC,
    .run_seconds = 20,
    .stats_period = DEFAULT_STATS_PERIOD,
    .promisc = true,
    .udp_only = false,
    .swap_mac = true,
    .rewrite_enable = false,
};

static uint16_t port_ids[RTE_MAX_ETHPORTS];
static uint16_t nb_ports_used;
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
    printf("  --run-seconds N          run duration, 0 means until Ctrl-C (default: %u)\n", cfg.run_seconds);
    printf("  --stats-period N         periodic stats interval in seconds (default: %u)\n", cfg.stats_period);
    printf("  --burst-size N           RX/TX burst size, max %u (default: %u)\n", MAX_BURST_SIZE, cfg.burst_size);
    printf("  --nb-mbuf N              mbuf count in pool (default: %u)\n", cfg.nb_mbuf);
    printf("  --rx-desc N              RX descriptors per port (default: %u)\n", cfg.rx_desc);
    printf("  --tx-desc N              TX descriptors per port (default: %u)\n", cfg.tx_desc);
    printf("  --promisc 0|1            disable/enable promiscuous mode (default: %u)\n", cfg.promisc ? 1U : 0U);
    printf("  --udp-only 0|1           drop IPv4 non-UDP packets, still pass ARP (default: %u)\n", cfg.udp_only ? 1U : 0U);
    printf("  --swap-mac 0|1           swap Ethernet src/dst before forwarding (default: %u)\n", cfg.swap_mac ? 1U : 0U);
    printf("  --rewrite 0|1            enable rewrite rules below (default: %u)\n", cfg.rewrite_enable ? 1U : 0U);
    printf("  --rewrite-src-mac MAC    set Ethernet source MAC\n");
    printf("  --rewrite-dst-mac MAC    set Ethernet destination MAC\n");
    printf("  --rewrite-src-ip IPv4    set IPv4 source address\n");
    printf("  --rewrite-dst-ip IPv4    set IPv4 destination address\n");
    printf("  --rewrite-src-port PORT  set UDP source port\n");
    printf("  --rewrite-dst-port PORT  set UDP destination port\n");
    printf("  --help                   show this help\n");
}

/* ========== 解析工具函数 ========== */
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

static int parse_bool_arg(const char *name, const char *value, bool *out)
{
    uint32_t v;

    if (parse_u32(name, value, &v) < 0) {
        return -1;
    }
    if (v > 1) {
        fprintf(stderr, "%s must be 0 or 1\n", name);
        return -1;
    }
    *out = (v == 1);
    return 0;
}

/* ========== 解析 MAC 地址 ========== */
static int parse_mac(const char *text, struct rte_ether_addr *addr)
{
    unsigned int b[6];
    int n;

    if (text == NULL || addr == NULL) {
        return -1;
    }

    n = sscanf(text, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    if (n != 6) {
        fprintf(stderr, "invalid MAC address: %s\n", text);
        return -1;
    }

    for (int i = 0; i < 6; i++) {
        if (b[i] > 0xff) {
            fprintf(stderr, "invalid MAC byte in: %s\n", text);
            return -1;
        }
        addr->addr_bytes[i] = (uint8_t)b[i];
    }

    return 0;
}

/* ========== 解析 IPv4 地址 ========== */
static int parse_ipv4(const char *text, uint32_t *addr)
{
    struct in_addr a;

    if (inet_pton(AF_INET, text, &a) != 1) {
        fprintf(stderr, "invalid IPv4 address: %s\n", text);
        return -1;
    }

    *addr = a.s_addr;
    return 0;
}

/* ========== 解析 UDP 端口 ========== */
static int parse_udp_port(const char *name, const char *text, uint16_t *port_be)
{
    uint32_t v;

    if (parse_u32(name, text, &v) < 0) {
        return -1;
    }
    if (v == 0 || v > 65535) {
        fprintf(stderr, "%s must be 1..65535\n", name);
        return -1;
    }

    /* 转换为网络字节序（大端序）*/
    *port_be = rte_cpu_to_be_16((uint16_t)v);
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
            usage("fastpath-lite");
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
            if (++i >= argc || parse_bool_arg(arg, argv[i], &cfg.promisc) < 0)
                return -1;
        } else if (strcmp(arg, "--udp-only") == 0) {
            if (++i >= argc || parse_bool_arg(arg, argv[i], &cfg.udp_only) < 0)
                return -1;
        } else if (strcmp(arg, "--swap-mac") == 0) {
            if (++i >= argc || parse_bool_arg(arg, argv[i], &cfg.swap_mac) < 0)
                return -1;
        } else if (strcmp(arg, "--rewrite") == 0) {
            if (++i >= argc || parse_bool_arg(arg, argv[i], &cfg.rewrite_enable) < 0)
                return -1;
        } else if (strcmp(arg, "--rewrite-src-mac") == 0) {
            if (++i >= argc || parse_mac(argv[i], &cfg.rewrite_src_mac) < 0)
                return -1;
            cfg.set_src_mac = true;
            cfg.rewrite_enable = true;
        } else if (strcmp(arg, "--rewrite-dst-mac") == 0) {
            if (++i >= argc || parse_mac(argv[i], &cfg.rewrite_dst_mac) < 0)
                return -1;
            cfg.set_dst_mac = true;
            cfg.rewrite_enable = true;
        } else if (strcmp(arg, "--rewrite-src-ip") == 0) {
            if (++i >= argc || parse_ipv4(argv[i], &cfg.rewrite_src_ip) < 0)
                return -1;
            cfg.set_src_ip = true;
            cfg.rewrite_enable = true;
        } else if (strcmp(arg, "--rewrite-dst-ip") == 0) {
            if (++i >= argc || parse_ipv4(argv[i], &cfg.rewrite_dst_ip) < 0)
                return -1;
            cfg.set_dst_ip = true;
            cfg.rewrite_enable = true;
        } else if (strcmp(arg, "--rewrite-src-port") == 0) {
            if (++i >= argc || parse_udp_port(arg, argv[i], &cfg.rewrite_src_port) < 0)
                return -1;
            cfg.set_src_port = true;
            cfg.rewrite_enable = true;
        } else if (strcmp(arg, "--rewrite-dst-port") == 0) {
            if (++i >= argc || parse_udp_port(arg, argv[i], &cfg.rewrite_dst_port) < 0)
                return -1;
            cfg.set_dst_port = true;
            cfg.rewrite_enable = true;
        } else if (strncmp(arg, "--", 2) == 0) {
            fprintf(stderr, "unknown APP option: %s\n", arg);
            usage("fastpath-lite");
            return -1;
        }
    }

    if (cfg.stats_period == 0) {
        cfg.stats_period = DEFAULT_STATS_PERIOD;
    }

    return 0;
}

/* ========== 打印 MAC/IPv4 地址 ========== */
static void print_mac_addr(const char *prefix, const struct rte_ether_addr *mac)
{
    printf("%s" RTE_ETHER_ADDR_PRT_FMT, prefix, RTE_ETHER_ADDR_BYTES(mac));
}

static void print_ipv4_addr(const char *prefix, uint32_t addr_be)
{
    char buf[INET_ADDRSTRLEN];
    struct in_addr a;

    a.s_addr = addr_be;
    if (inet_ntop(AF_INET, &a, buf, sizeof(buf)) == NULL) {
        snprintf(buf, sizeof(buf), "<invalid>");
    }
    printf("%s%s", prefix, buf);
}

/* ========== 打印应用配置 ========== */
static void print_app_config(void)
{
    printf("fastpath-lite config: nb_mbuf=%u mbuf_cache=%u rx_desc=%u tx_desc=%u burst=%u run_seconds=%u stats_period=%u\n",
           cfg.nb_mbuf, cfg.mbuf_cache, cfg.rx_desc, cfg.tx_desc,
           cfg.burst_size, cfg.run_seconds, cfg.stats_period);
    printf("policy: promisc=%u udp_only=%u swap_mac=%u rewrite_enable=%u\n",
           cfg.promisc ? 1U : 0U, cfg.udp_only ? 1U : 0U,
           cfg.swap_mac ? 1U : 0U, cfg.rewrite_enable ? 1U : 0U);

    if (!cfg.rewrite_enable) {
        return;
    }

    printf("rewrite rules:");
    if (cfg.set_src_mac) print_mac_addr(" src_mac=", &cfg.rewrite_src_mac);
    if (cfg.set_dst_mac) print_mac_addr(" dst_mac=", &cfg.rewrite_dst_mac);
    if (cfg.set_src_ip) print_ipv4_addr(" src_ip=", cfg.rewrite_src_ip);
    if (cfg.set_dst_ip) print_ipv4_addr(" dst_ip=", cfg.rewrite_dst_ip);
    if (cfg.set_src_port) printf(" src_port=%u", rte_be_to_cpu_16(cfg.rewrite_src_port));
    if (cfg.set_dst_port) printf(" dst_port=%u", rte_be_to_cpu_16(cfg.rewrite_dst_port));
    printf("\n");
}

/* ========== 打印端口 MAC 地址 ========== */
static void print_port_mac(uint16_t portid)
{
    struct rte_ether_addr mac;
    int ret = rte_eth_macaddr_get(portid, &mac);

    if (ret == 0) {
        printf("port %u MAC: " RTE_ETHER_ADDR_PRT_FMT "\n",
               portid, RTE_ETHER_ADDR_BYTES(&mac));
    } else {
        printf("port %u MAC: unavailable, ret=%d\n", portid, ret);
    }
}

/* ========== 获取配对端口（与 l2fwd-lite 相同）============ */
static uint16_t paired_port(uint16_t portid)
{
    for (uint16_t i = 0; i < nb_ports_used; i++) {
        if (port_ids[i] != portid) {
            continue;
        }
        if ((i % 2) == 0) {
            if (i + 1 < nb_ports_used) {
                return port_ids[i + 1];
            }
        } else {
            return port_ids[i - 1];
        }
    }
    return RTE_MAX_ETHPORTS;
}

/* ========== MAC 地址交换（条件版本）============ */
static void maybe_swap_eth_addr(struct rte_ether_hdr *eth)
{
    struct rte_ether_addr tmp;

    if (!cfg.swap_mac) {
        return;
    }

    tmp = eth->src_addr;
    eth->src_addr = eth->dst_addr;
    eth->dst_addr = tmp;
}

/* ========== 处理 IPv4/UDP 包（核心分类与 rewrite）============
 *
 * 这是 fastpath-lite 相对于 l2fwd-lite 的核心差异
 *
 * 流程：
 *   1. 检查 IPv4 头部长度
 *   2. 判断是 UDP 还是其他协议
 *   3. 如果启用 rewrite，替换 MAC/IPv4/UDP 头部字段
 *   4. 重新计算 IPv4 checksum
 */
static bool handle_ipv4_udp(uint16_t src_portid, struct rte_mbuf *m, struct rte_ether_hdr *eth)
{
    struct rte_ipv4_hdr *ipv4;
    uint8_t ihl;
    bool is_udp;
    bool changed_ip = false;
    bool changed_udp = false;

    /* 检查包长度是否足够容纳 IPv4 头 */
    if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr)) {
        sw_stats[src_portid].drop_short++;
        return false;
    }

    /* 获取 IPv4 头部指针 */
    ipv4 = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));

    /* 解析 IHL（Internet Header Length），单位是 4 字节 */
    ihl = (uint8_t)((ipv4->version_ihl & 0x0fU) * 4U);
    if (ihl < sizeof(struct rte_ipv4_hdr) ||
        rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr) + ihl) {
        sw_stats[src_portid].drop_short++;
        return false;
    }

    sw_stats[src_portid].ipv4_packets++;

    /* 判断上层协议是否为 UDP */
    is_udp = (ipv4->next_proto_id == IPPROTO_UDP);
    if (!is_udp) {
        sw_stats[src_portid].non_udp_packets++;
        if (cfg.udp_only) {
            /* udp_only 模式：丢弃非 UDP 包 */
            sw_stats[src_portid].drop_non_udp++;
            return false;
        }
        maybe_swap_eth_addr(eth);
        return true;
    }

    sw_stats[src_portid].udp_packets++;

    /* ========== UDP rewrite 处理 ========== */
    if (cfg.rewrite_enable) {
        struct rte_udp_hdr *udp;

        /* 检查 UDP 头部长度 */
        if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr) + ihl + sizeof(struct rte_udp_hdr)) {
            sw_stats[src_portid].drop_short++;
            return false;
        }

        /* L2 rewrite: MAC 地址替换 */
        if (cfg.set_src_mac) {
            eth->src_addr = cfg.rewrite_src_mac;
        }
        if (cfg.set_dst_mac) {
            eth->dst_addr = cfg.rewrite_dst_mac;
        }
        /* 如果没有指定 MAC rewrite，则交换原有 MAC */
        if (!cfg.set_src_mac && !cfg.set_dst_mac) {
            maybe_swap_eth_addr(eth);
        }

        /* L3 rewrite: IPv4 地址替换 */
        if (cfg.set_src_ip) {
            ipv4->src_addr = cfg.rewrite_src_ip;
            changed_ip = true;
        }
        if (cfg.set_dst_ip) {
            ipv4->dst_addr = cfg.rewrite_dst_ip;
            changed_ip = true;
        }

        /* L4 rewrite: UDP 端口替换 */
        udp = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + ihl);
        if (cfg.set_src_port) {
            udp->src_port = cfg.rewrite_src_port;
            changed_udp = true;
        }
        if (cfg.set_dst_port) {
            udp->dst_port = cfg.rewrite_dst_port;
            changed_udp = true;
        }

        /* 重新计算 IPv4 checksum */
        if (changed_ip) {
            ipv4->hdr_checksum = 0;
            ipv4->hdr_checksum = rte_ipv4_cksum(ipv4);
        }
        /* UDP checksum 设为 0（允许不校验）*/
        if (changed_ip || changed_udp) {
            udp->dgram_cksum = 0;
        }

        sw_stats[src_portid].rewrite_packets++;
        return true;
    }

    maybe_swap_eth_addr(eth);
    return true;
}

/* ========== 分类与路由主函数 ==========
 *
 * 解析 Ethernet 类型，决定如何处理：
 *   - ARP: 直接交换 MAC 后转发
 *   - IPv4: 交给 handle_ipv4_udp 处理
 *   - 其他: 如果 udp_only 开启则丢弃，否则交换 MAC 后转发
 */
static bool classify_and_rewrite(uint16_t src_portid, struct rte_mbuf *m)
{
    struct rte_ether_hdr *eth;
    uint16_t ether_type;

    if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr)) {
        sw_stats[src_portid].drop_short++;
        return false;
    }

    eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

    /* 转换为主机字节序比较 */
    ether_type = rte_be_to_cpu_16(eth->ether_type);

    if (ether_type == RTE_ETHER_TYPE_ARP) {
        /* ARP: 统计并交换 MAC 后直接转发 */
        sw_stats[src_portid].arp_packets++;
        maybe_swap_eth_addr(eth);
        return true;
    }

    if (ether_type == RTE_ETHER_TYPE_IPV4) {
        /* IPv4: 进入 UDP/IPv4 处理流程 */
        return handle_ipv4_udp(src_portid, m, eth);
    }

    /* 非 IPv4 包 */
    sw_stats[src_portid].non_udp_packets++;
    if (cfg.udp_only) {
        sw_stats[src_portid].drop_non_udp++;
        return false;
    }

    maybe_swap_eth_addr(eth);
    return true;
}

/* ========== 打印软件统计 ========== */
static void print_sw_stats(void)
{
    printf("\n==== fastpath-lite software stats ====\n");
    for (uint16_t i = 0; i < nb_ports_used; i++) {
        const uint16_t portid = port_ids[i];
        const struct port_sw_stats *s = &sw_stats[portid];

        printf("port %u: rx=%" PRIu64 " rx_bytes=%" PRIu64
               " tx=%" PRIu64 " tx_bytes=%" PRIu64 " tx_failed=%" PRIu64
               " arp=%" PRIu64 " ipv4=%" PRIu64 " udp=%" PRIu64 " non_udp=%" PRIu64
               " rewrite=%" PRIu64 " drop_short=%" PRIu64 " drop_non_udp=%" PRIu64 " drop_no_peer=%" PRIu64 "\n",
               portid,
               s->rx_packets, s->rx_bytes,
               s->tx_packets, s->tx_bytes, s->tx_failed,
               s->arp_packets, s->ipv4_packets, s->udp_packets, s->non_udp_packets,
               s->rewrite_packets, s->drop_short, s->drop_non_udp, s->drop_no_peer);
    }
    printf("======================================\n");
}

/* ========== 打印网卡硬件统计 ========== */
static void print_ethdev_stats(void)
{
    printf("\n==== rte_eth_stats ====\n");
    for (uint16_t i = 0; i < nb_ports_used; i++) {
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

/* ========== 初始化单个端口（与 l2fwd-lite 相同）============ */
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

    ret = rte_eth_dev_info_get(portid, &dev_info);
    if (ret != 0) {
        printf("rte_eth_dev_info_get(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
        port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
    }

    ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
    if (ret < 0) {
        printf("rte_eth_dev_configure(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &rx_desc, &tx_desc);
    if (ret < 0) {
        printf("rte_eth_dev_adjust_nb_rx_tx_desc(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    socket_id = rte_eth_dev_socket_id(portid);
    if (socket_id < 0) {
        socket_id = rte_socket_id();
    }

    ret = rte_eth_rx_queue_setup(portid, 0, rx_desc, (unsigned int)socket_id, NULL, mbuf_pool);
    if (ret < 0) {
        printf("rte_eth_rx_queue_setup(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    ret = rte_eth_tx_queue_setup(portid, 0, tx_desc, (unsigned int)socket_id, &txconf);
    if (ret < 0) {
        printf("rte_eth_tx_queue_setup(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    ret = rte_eth_dev_start(portid);
    if (ret < 0) {
        printf("rte_eth_dev_start(port=%u) failed: %d\n", portid, ret);
        return ret;
    }

    if (cfg.promisc) {
        ret = rte_eth_promiscuous_enable(portid);
        if (ret != 0) {
            printf("warning: rte_eth_promiscuous_enable(port=%u) failed: %d\n", portid, ret);
        }
    }

    printf("port %u started: rx_desc=%u tx_desc=%u socket=%d driver=%s\n",
           portid, rx_desc, tx_desc, socket_id,
           dev_info.driver_name ? dev_info.driver_name : "unknown");
    print_port_mac(portid);

    return 0;
}

/* ========== 初始化所有端口 ========== */
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
        printf("notice: only one port is available; running RX/classify/free smoke mode, no forwarding peer.\n");
    } else if ((nb_ports_used % 2) != 0) {
        printf("notice: odd number of ports; the last port will RX/classify/free because it has no peer.\n");
    }

    return 0;
}

/* ========== 转发主循环 ==========
 *
 * 与 l2fwd-lite 的区别：
 *   - 不直接交换 MAC，而是调用 classify_and_rewrite()
 *   - classify_and_rewrite() 会根据 ether_type 分类处理
 *   - 被 classify_and_rewrite() 标记为 drop 的包直接释放
 */
static void forwarding_loop(void)
{
    const uint64_t hz = rte_get_timer_hz();
    const uint64_t start_tsc = rte_get_timer_cycles();
    uint64_t next_stats_tsc = start_tsc + (uint64_t)cfg.stats_period * hz;
    struct rte_mbuf *rx_pkts[MAX_BURST_SIZE];
    struct rte_mbuf *tx_pkts[MAX_BURST_SIZE];
    uint32_t tx_len[MAX_BURST_SIZE];

    printf("enter fastpath loop: run_seconds=%u stats_period=%u burst=%u lcore=%u\n",
           cfg.run_seconds, cfg.stats_period, cfg.burst_size, rte_lcore_id());

    while (!force_quit) {
        const uint64_t now = rte_get_timer_cycles();

        if (cfg.run_seconds > 0 && now - start_tsc >= (uint64_t)cfg.run_seconds * hz) {
            printf("run_seconds reached, stopping...\n");
            break;
        }

        for (uint16_t i = 0; i < nb_ports_used; i++) {
            const uint16_t src = port_ids[i];
            const uint16_t dst = paired_port(src);
            uint16_t nb_rx;
            uint16_t nb_forward = 0;

            /* RX: 从 src 端口接收包 */
            nb_rx = rte_eth_rx_burst(src, 0, rx_pkts, cfg.burst_size);
            if (nb_rx == 0) {
                continue;
            }

            sw_stats[src].rx_packets += nb_rx;
            for (uint16_t j = 0; j < nb_rx; j++) {
                const uint32_t len = rte_pktmbuf_pkt_len(rx_pkts[j]);
                sw_stats[src].rx_bytes += len;

                /* 分类与 rewrite 处理 */
                if (classify_and_rewrite(src, rx_pkts[j])) {
                    tx_pkts[nb_forward] = rx_pkts[j];
                    tx_len[nb_forward] = len;
                    nb_forward++;
                } else {
                    /* 被分类器标记为丢弃的包 */
                    rte_pktmbuf_free(rx_pkts[j]);
                }
            }

            if (nb_forward == 0) {
                continue;
            }

            /* 无配对端口：释放所有待发送的包 */
            if (dst == RTE_MAX_ETHPORTS) {
                for (uint16_t j = 0; j < nb_forward; j++) {
                    rte_pktmbuf_free(tx_pkts[j]);
                }
                sw_stats[src].drop_no_peer += nb_forward;
                continue;
            }

            /* TX: 发送到配对端口 */
            const uint16_t nb_tx = rte_eth_tx_burst(dst, 0, tx_pkts, nb_forward);
            sw_stats[dst].tx_packets += nb_tx;
            for (uint16_t j = 0; j < nb_tx; j++) {
                sw_stats[dst].tx_bytes += tx_len[j];
            }

            /* TX 队列满导致发送失败：释放剩余 mbuf */
            if (nb_tx < nb_forward) {
                sw_stats[dst].tx_failed += nb_forward - nb_tx;
                for (uint16_t j = nb_tx; j < nb_forward; j++) {
                    rte_pktmbuf_free(tx_pkts[j]);
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
    for (uint16_t i = 0; i < nb_ports_used; i++) {
        const uint16_t portid = port_ids[i];
        printf("stopping port %u\n", portid);
        rte_eth_dev_stop(portid);
        rte_eth_dev_close(portid);
    }
}

/* ========== 主函数 ==========
 *
 * DPDK 应用入口顺序（与 l2fwd-lite 相同）
 */
int main(int argc, char **argv)
{
    struct rte_mempool *mbuf_pool;
    int ret;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* ========== 1. EAL 初始化 ========== */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "rte_eal_init failed\n");
    }

    argc -= ret;
    argv += ret;

    /* ========== 2. 解析应用参数 ========== */
    ret = parse_app_args(argc, argv);
    if (ret > 0) {
        rte_eal_cleanup();
        return 0;
    }
    if (ret < 0) {
        rte_eal_cleanup();
        return 1;
    }

    print_app_config();

    /* ========== 3. 创建 mbuf 内存池 ========== */
    mbuf_pool = rte_pktmbuf_pool_create("fastpath_lite_mbuf_pool",
                                        cfg.nb_mbuf,
                                        cfg.mbuf_cache,
                                        0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "rte_pktmbuf_pool_create failed\n");
    }

    /* ========== 4. 初始化所有端口 ========== */
    ret = init_all_ports(mbuf_pool);
    if (ret != 0) {
        rte_eal_cleanup();
        return 1;
    }

    /* ========== 5. 进入转发主循环 ========== */
    forwarding_loop();

    /* ========== 6. 打印统计 ========== */
    print_sw_stats();
    print_ethdev_stats();

    /* ========== 7. 停止端口 ========== */
    stop_all_ports();

    /* ========== 8. 清理 EAL ========== */
    ret = rte_eal_cleanup();
    if (ret != 0) {
        printf("rte_eal_cleanup returned %d\n", ret);
    }

    printf("bye\n");
    return 0;
}
