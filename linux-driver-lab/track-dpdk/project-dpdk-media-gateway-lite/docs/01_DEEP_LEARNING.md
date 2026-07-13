# 01_DEEP_LEARNING — DPDK 用户态媒体网关深度拆解

> 本文档对 media-gateway-lite 进行逐层深度拆解，覆盖从 DPDK 基本原理到逐行代码实现。
> 阅读后你将理解：为什么需要内核旁路、DPDK 如何组织内存和报文、poll mode 如何驱动数据面、
> 一个媒体网关的五元组匹配和 rewrite 引擎如何工作。

---

## 目录

1. [前置知识：为什么需要 DPDK](#1-前置知识为什么需要-dpdk)
2. [DPDK 核心概念拆解](#2-dpdk-核心概念拆解)
3. [系统架构与模块关系](#3-系统架构与模块关系)
4. [初始化流程：从 main() 到进入 poll loop](#4-初始化流程从-main-到进入-poll-loop)
5. [报文处理管线：逐包深度追踪](#5-报文处理管线逐包深度追踪)
6. [规则引擎：匹配与 rewrite](#6-规则引擎匹配与-rewrite)
7. [内存模型与 mbuf 生命周期](#7-内存模型与-mbuf-生命周期)
8. [统计系统设计](#8-统计系统设计)
9. [测试策略与 pcap PMD 方案](#9-测试策略与-pcap-pmd-方案)
10. [关键踩坑记录](#10-关键踩坑记录)

---

## 1. 前置知识：为什么需要 DPDK

### 1.1 内核网络栈的瓶颈

在标准 Linux 内核中，一个 UDP 报文从网卡到用户态程序的路径是：

```text
┌──────────┐    DMA     ┌─────────────┐    skb     ┌──────────────┐   recvmsg()   ┌──────────┐
│  NIC HW  │ ────────→  │  NAPI poll   │ ────────→ │  协议栈      │ ────────────→ │  应用    │
│ (Ring B) │            │  (软中断)     │           │  IP→UDP→SK  │              │  (用户态) │
└──────────┘            └─────────────┘           └──────────────┘              └──────────┘
                              │                         │
                         per-IRQ                 socket buffer
                         context switch           copy (kernel→user)
```

每个报文经过：
- **硬中断** → NAPI 软中断调度
- **软中断上下文**（ksoftirqd）→ skb 分配
- **协议栈遍历**（L2→L3→L4，netfilter 钩子）
- **socket buffer** → `copy_to_user()` 拷贝到用户态
- **系统调用开销**（`recvmsg` / `sendmsg` 的上下文切换）

对于 10 GbE 最小 64-byte frame，计入 preamble/SFD 和 IFG 后线速约为 14.88 Mpps。通用内核 socket 路径在这类小包高包速场景下可能消耗大量 CPU；是否达到瓶颈取决于硬件、内核配置、XDP/AF_XDP、协议和业务处理，不能绝对化表述为“内核一定处理不过来”。

### 1.2 DPDK 的解法：内核旁路 (Kernel Bypass)

```text
                    ┌─────────────────────────────────┐
                    │         DPDK 应用 (用户态)        │
                    │  ┌──────────┐  ┌──────────────┐  │
                    │  │ RX/TX    │  │  业务逻辑     │  │
                    │  │ poll mode│  │  (网关/UDP处理)│  │
                    │  └────┬─────┘  └──────────────┘  │
                    │       │                           │
                    │  ┌────▼──────────────────────┐    │
                    │  │    mbuf pool (hugepages)   │    │
                    │  └───────────────────────────┘    │
                    └──────────────┬────────────────────┘
                                   │ PMD (用户态驱动)
                                   │ UIO / VFIO 映射
                    ┌──────────────▼────────────────────┐
                    │           内核空间                  │
                    │  ┌──────────────────────────┐      │
                    │  │  uio_pci_generic / vfio  │      │
                    │  │  (仅做中断→userspace通知)  │      │
                    │  └──────────────────────────┘      │
                    └──────────────┬────────────────────┘
                                   │ PCIe MMIO
                    ┌──────────────▼────────────────────┐
                    │          NIC 硬件                   │
                    │  ┌──────┐  ┌──────┐  ┌──────┐     │
                    │  │RX Q0 │  │TX Q0 │  │  ... │     │
                    │  └──────┘  └──────┘  └──────┘     │
                    └─────────────────────────────────┘
```

核心思想：
1. **用户态驱动 (PMD)**：绕过内核协议栈，直接在用户态操作网卡寄存器
2. **Poll Mode**：不靠中断，用轮询（`rx_burst` / `tx_burst`），消除中断开销
3. **Hugepages**：2MB/1GB 大页，减少 TLB miss，mbuf 池常驻物理内存
4. **零拷贝**：mbuf 在用户态和 NIC DMA 之间共享，无内核→用户态拷贝
5. **Run-to-completion**：单核处理整条 pipeline，无锁、无上下文切换

### 1.3 适用场景

| 场景 | 吞吐需求 | 是否适合 DPDK |
|------|---------|--------------|
| 普通 Web 服务器 | ~1Gbps, <100K pps | 不需要 |
| DNS / CDN edge | ~10Gbps | 可选 |
| **媒体网关 / SBC** | **10-100Gbps, 千万级 pps** | **适合** |
| 5G UPF / vBNG | 100Gbps+ | 标准方案 |
| 高频交易 | 微秒级延迟 | 标准方案 |

---

## 2. DPDK 核心概念拆解

### 2.1 EAL (Environment Abstraction Layer)

EAL 是 DPDK 的"操作系统"——在 `main()` 中最先调用，负责：

```c
int ret = rte_eal_init(argc, argv);
// 这一行背后做了：
//   1. 解析 CPU 拓扑 (lcore / NUMA / cache)
//   2. 初始化 hugepage 内存
//   3. 设置 PCI 总线扫描
//   4. 加载 PMD 驱动
//   5. 初始化 trace / telemetry
//   6. 消费 EAL 相关参数，返回剩余 argc
argc -= ret;  // EAL 吃掉的参数数
argv += ret;  // 剩余参数给 APP 使用
```

### 2.2 Hugepages

```
标准 4KB 页：              Hugepages 2MB 页：
┌──┬──┬──┬──┬──┬──┬──┐     ┌──────────────────────┐
│4K│4K│4K│4K│4K│4K│4K│     │        2MB            │
└──┴──┴──┴──┴──┴──┴──┘     └──────────────────────┘
   x 512 = 2MB                 1 page = 2MB

TLB 条目:
  4K 页 → 512 个 TLB 条目 (覆盖 2MB)
  2MB 页 → 1 个 TLB 条目 (覆盖 2MB)
```

典型真实 NIC DPDK 路径使用 hugepage-backed memory，扩大 TLB 覆盖并便于稳定的 DMA/IOVA 映射。每个 hugepage 是大粒度页，但不能假设整个 mempool 是单一物理连续块。`--no-huge` 适合部分 vdev 功能测试，不能据此推断真实 NIC DMA 配置或性能等价。

### 2.3 mbuf (Message Buffer)

`rte_mbuf` 是 DPDK 的"宇宙中心"——所有报文都用它承载：

```text
                    rte_mbuf 结构 (128 bytes)
    ┌──────────────────────────────────────────────────────┐
    │  metadata (refcnt, port, ol_flags, timestamp, ...)   │ ← 控制信息
    ├──────────────────────────────────────────────────────┤
    │  headroom (128 bytes)                                │ ← 可在报文前插入 header
    │  ┌──────────────────────────────────────────────┐    │
    │  │            packet data                        │    │ ← 实际报文数据
    │  │  [Ethernet hdr][IPv4 hdr][UDP hdr][payload]  │    │
    │  └──────────────────────────────────────────────┘    │
    │  tailroom (可追加数据)                                │
    └──────────────────────────────────────────────────────┘

    buf_addr ──────→ headroom 起点
    data_off ──────→ packet data 起点
    pkt_len  ──────→ 报文总长度
    data_len ──────→ 当前 segment 数据长度
```

关键 API：

```c
// 从 mbuf 获取报文头指针
struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

// 获取报文总长度（注意：tx_burst 后不能访问！）
uint32_t len = rte_pktmbuf_pkt_len(m);

// 从偏移位置获取指针
struct rte_ipv4_hdr *ip = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));

// 释放 mbuf
rte_pktmbuf_free(m);
```

### 2.4 mempool

mempool 是预分配的 mbuf 池，环形缓存（lockless ring）：

```text
    rte_mempool ("media_gateway_mbuf_pool")
    ┌─────────────────────────────────────────┐
    │  ┌─────┐ ┌─────┐ ┌─────┐     ┌─────┐   │
    │  │mbuf │→│mbuf │→│mbuf │→...→│mbuf │   │  ← 默认 8192 个
    │  └─────┘ └─────┘ └─────┘     └─────┘   │
    │    ↑                           ↓       │
    │    └──── lcore cache (250) ────┘       │  ← per-lcore 缓存，无锁
    └─────────────────────────────────────────┘
```

创建方式：

```c
pool = rte_pktmbuf_pool_create(
    "media_gateway_mbuf_pool",
    8192,          // nb_mbuf: 总 mbuf 数
    250,           // cache: per-lcore 缓存大小
    0,             // priv_size: 私有数据区
    RTE_MBUF_DEFAULT_BUF_SIZE,  // 2048 + 128 = 2176 字节
    rte_socket_id()  // NUMA 节点
);
```

### 2.5 Poll Mode Driver (PMD)

不同于内核驱动的中断模型，DPDK PMD 用轮询（poll）：

```c
// 收包：一次收一批（burst），最多 burst_size 个
uint16_t nb_rx = rte_eth_rx_burst(portid, queue_id, pkts, burst_size);
// 返回值：实际收到的包数（0 表示没包，不需要阻塞等待）

// 发包：一次发一批
uint16_t sent = rte_eth_tx_burst(portid, queue_id, pkts, nb_pkts);
// 返回值：实际发出的包数（可能 < nb_pkts，说明 TX ring 满了）
```

**批量处理 (burst) 是 DPDK 性能的关键**——分摊函数调用开销、利用 CPU 缓存局部性、减少内存屏障。

---

## 3. 系统架构与模块关系

### 3.1 组件图

```text
                            ┌──────────────────────┐
                            │     main.c            │
                            │  ┌────────────────┐   │
                            │  │ signal handler  │   │
                            │  │ force_quit      │   │
                            │  └────────────────┘   │
                            │  ┌────────────────┐   │
                            │  │ run_loop()      │   │
                            │  │  poll + fwd     │   │
                            │  └───┬────────────┘   │
                            └──────┼──────────────────┘
                                   │
        ┌──────────────────────────┼──────────────────────────┐
        │                          │                           │
        ▼                          ▼                           ▼
┌──────────────┐     ┌─────────────────────┐     ┌──────────────────┐
│gateway_port  │     │ gateway_packet      │     │ gateway_stats    │
│──────────────│     │─────────────────────│     │──────────────────│
│discover_ports│     │gw_packet_process()  │     │gw_stats_print()  │
│init_port()   │     │  ├─ ARP → route_l2  │     │gw_stats_reset()  │
│stop_ports()  │     │  ├─ IPv4→ check UDP │     │ethdev_stats_print│
└──────┬───────┘     │  ├─ UDP → rule match│     └──────────────────┘
       │             │  └─ rewrite + fwd   │
       │             └────────┬────────────┘
       │                      │
       ▼                      ▼
┌──────────────┐     ┌─────────────────────┐
│ DPDK ethdev  │     │ gateway_rule        │
│ (硬件/虚拟)   │     │─────────────────────│
└──────────────┘     │gw_find_udp_rule()   │
                     │gw_find_l2_rule()    │
                     │gw_rule_apply_rewrite│
                     │rule_udp_match()     │
                     └──────────┬──────────┘
                                │
                                ▼
                     ┌─────────────────────┐
                     │ gateway_config      │
                     │─────────────────────│
                     │gw_config_parse_args │
                     │gw_rules_prepare_def │
                     │gw_config_print()    │
                     └─────────────────────┘
```

### 3.2 类图（C 结构体关系）

```text
┌─────────────────┐         ┌──────────────────┐
│   gw_config     │         │  gw_runtime_stats│
├─────────────────┤         ├──────────────────┤
│ nb_mbuf: u32    │         │ port[64]:        │
│ mbuf_cache: u32 │         │  gw_port_stats   │
│ burst_size: u16 │         └────────┬─────────┘
│ rx_desc: u16    │                  │
│ tx_desc: u16    │     ┌────────────▼─────────────┐
│ run_seconds:u32 │     │    gw_port_stats          │
│ stats_period:u32│     ├───────────────────────────┤
│ promisc: bool   │     │ rx, tx, drops            │
│ udp_only: bool  │     │ arp, ipv4, udp, non_udp  │
│ swap_mac: bool  │     │ rewrite                   │
│ strict_rules:bool│    │ drop_short/drop_non_udp/  │
│ nb_rules: u16   │     │ drop_no_route             │
│ rules[4] ───────┼──┐  │ rule_hit[4], rule_bytes[4]│
└─────────────────┘  │  │ rule_rewrite[4]           │
                     │  └───────────────────────────┘
                     │
        ┌────────────▼──────────────┐
        │      gw_rule              │
        ├───────────────────────────┤
        │ enabled: bool             │
        │ name[32]: char            │
        │ in_port → out_port: u16   │
        │                           │
        │ match_src_ip / match_dst_ip│
        │ match_src_port / dst_port │
        │ src_ip, dst_ip: u32 (BE)  │
        │ src_port, dst_port:u16(BE)│
        │                           │
        │ set_src_mac / set_dst_mac │
        │ set_src_ip / set_dst_ip   │
        │ set_src_port/set_dst_port │
        │ rewrite_* values          │
        └───────────────────────────┘
```

### 3.3 数据流概览

```text
         ┌─────────────────────────────────────────────┐
         │              run_loop() 主循环               │
         │                                             │
         │  ┌──────────────────────────────────────┐   │
         │  │   for each port (portid = 0..N-1):   │   │
         │  │       rx_burst(portid, pkts, 32)     │   │
         │  │       if nb_rx == 0 → continue       │   │
         │  │                                      │   │
         │  │   ┌──────────────────────────────┐   │   │
         │  │   │ for each pkt (j = 0..nb_rx): │   │   │
         │  │   │                              │   │   │
         │  │   │  ┌─────────────────────┐     │   │   │
         │  │   │  │stats.port[pid].rx++ │     │   │   │
         │  │   │  └────────┬────────────┘     │   │   │
         │  │   │           │                  │   │   │
         │  │   │  ┌────────▼────────────┐     │   │   │
         │  │   │  │ gw_packet_process() │     │   │   │
         │  │   │  │ → classify          │     │   │   │
         │  │   │  │ → rule match        │     │   │   │
         │  │   │  │ → rewrite           │     │   │   │
         │  │   │  └────────┬───────────┘     │   │   │
         │  │   │           │ result           │   │   │
         │  │   │  ┌────────▼────────────┐     │   │   │
         │  │   │  │ forward_or_drop()   │     │   │   │
         │  │   │  │ → tx_burst OR free  │     │   │   │
         │  │   │  └─────────────────────┘     │   │   │
         │  │   └──────────────────────────────┘   │   │
         │  └──────────────────────────────────────┘   │
         │                                             │
         │  ┌──────────────────────────────────────┐   │
         │  │ if (now >= next_stats):              │   │
         │  │     gw_stats_print()                 │   │
         │  │     gw_ethdev_stats_print()          │   │
         │  └──────────────────────────────────────┘   │
         └─────────────────────────────────────────────┘
```

---

## 4. 初始化流程：从 main() 到进入 poll loop

### 4.1 初始化序列图

```text
main()                  DPDK EAL          mempool        ethdev         config
  │                        │                 │              │              │
  ├─gw_config_init()       │                 │              │              │
  │  (默认值设置)           │                 │              │              │
  │                        │                 │              │              │
  ├─signal(SIGINT/TERM)    │                 │              │              │
  │                        │                 │              │              │
  ├─rte_eal_init(argc,argv)│                 │              │              │
  │──────────────────────→ │                 │              │              │
  │                        │─扫描 PCI 设备    │              │              │
  │                        │─初始化 hugepages │              │              │
  │                        │─加载 PMD 驱动    │              │              │
  │  ret (剩余 argc)       │                 │              │              │
  │←────────────────────── │                 │              │              │
  │                        │                 │              │              │
  ├─gw_config_parse_args() │                 │              │              │
  │────────────────────────────────────────────────────────────────────→ │
  │  (解析 --rule0, --run-seconds, --udp-only 等)                        │
  │←──────────────────────────────────────────────────────────────────── │
  │                        │                 │              │              │
  ├─rte_pktmbuf_pool_create()                │              │              │
  │─────────────────────────────────────────→│              │              │
  │  pool ptr              │                 │              │              │
  │←─────────────────────────────────────────│              │              │
  │                        │                 │              │              │
  ├─gw_discover_ports()    │                 │              │              │
  │────────────────────────────────────────────────────────→│              │
  │  ports[] + nb_ports    │                 │              │              │
  │←────────────────────────────────────────────────────────│              │
  │                        │                 │              │              │
  ├─gw_rules_prepare_defaults()             │              │              │
  │  (如果无显式规则，自动双向)              │              │              │
  │                        │                 │              │              │
  ├─gw_config_print()      │                 │              │              │
  │  (打印最终配置)         │                 │              │              │
  │                        │                 │              │              │
  ├─gw_stats_reset()       │                 │              │              │
  │                        │                 │              │              │
  ├─for each port:         │                 │              │              │
  │  gw_init_port() ──────────────────────────────────────→│              │
  │    rte_eth_dev_configure(1RX+1TX)        │            │              │
  │    rte_eth_rx_queue_setup(pool)          │            │              │
  │    rte_eth_tx_queue_setup()              │            │              │
  │    rte_eth_dev_start()                   │            │              │
  │    rte_eth_promiscuous_enable()          │            │              │
  │  ←─────────────────────────────────────────────────────│              │
  │                        │                 │              │              │
  ├─run_loop()  ←── 进入主循环               │              │              │
```

### 4.2 端口发现与初始化细节

```c
// gw_discover_ports — 发现所有 DPDK ethdev 设备
int gw_discover_ports(uint16_t *ports, uint16_t max_ports, uint16_t *nb_ports) {
    uint16_t portid;
    uint16_t count = 0;
    RTE_ETH_FOREACH_DEV(portid) {  // 遍历所有已注册的 ethdev（物理 + vdev）
        if (count >= max_ports) break;
        ports[count++] = portid;
    }
    *nb_ports = count;
    return count > 0 ? 0 : -1;  // 至少需要一个端口
}

// gw_init_port — 初始化单个端口
int gw_init_port(uint16_t portid, struct rte_mempool *pool, const struct gw_config *cfg) {
    // 1. 配置 ethdev：1 个 RX 队列 + 1 个 TX 队列
    rte_eth_dev_configure(portid, 1, 1, &port_conf);

    // 2. 驱动可能调整 descriptor 数量（硬件限制）
    rte_eth_dev_adjust_nb_rx_tx_desc(portid, &rx_desc, &tx_desc);

    // 3. 设置 RX 队列（关联 mbuf pool）
    rte_eth_rx_queue_setup(portid, 0, rx_desc, socket_id, NULL, pool);

    // 4. 设置 TX 队列
    rte_eth_tx_queue_setup(portid, 0, tx_desc, socket_id, NULL);

    // 5. 启动端口
    rte_eth_dev_start(portid);

    // 6. 混杂模式（接收所有 MAC 的报文，网关/交换机标准配置）
    if (cfg->promisc) rte_eth_promiscuous_enable(portid);
}
```

---

## 5. 报文处理管线：逐包深度追踪

### 5.1 分类决策树

```text
                        rte_eth_rx_burst(portid)
                                │
                                ▼
                    ┌───────────────────────┐
                    │  检查 mbuf data_len    │
                    │  >= Ethernet hdr (14)? │
                    └───────────┬───────────┘
                                │ YES                    │ NO
                                ▼                        ▼
                    ┌───────────────────┐    ┌─────────────────┐
                    │ 读取 ether_type   │    │ drop_short++    │
                    │ (网络字节序→主机)  │    │ return DROP      │
                    └────────┬──────────┘    └─────────────────┘
                             │
            ┌────────────────┼────────────────┐
            │                │                │
            ▼                ▼                ▼
    ether_type ==      ether_type ==     其他 ether_type
    RTE_ETHER_TYPE_ARP  RTE_ETHER_TYPE_IPV4
            │                │                │
            ▼                ▼                ▼
    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
    │ arp++        │  │ ipv4++       │  │ non_udp++    │
    │ route_l2()   │  │ 检查 IP hdr  │  │ if udp_only  │
    │ swap MAC     │  │ IHL 有效性   │  │   drop_non_  │
    │ forward to   │  └──────┬───────┘  │   udp++      │
    │ out_port     │         │          │   return DROP │
    └──────────────┘         │          │ else          │
                             │          │   route_l2() │
                ┌────────────┼──────────┐└──────────────┘
                │            │          │
                ▼            ▼          ▼
        protocol==UDP   protocol!=UDP  (ARP 已在上层处理)
                │            │
                ▼            ▼
        ┌──────────────┐  ┌──────────────┐
        │ udp++        │  │ non_udp++    │
        │ 检查 UDP hdr │  │ if udp_only  │
        │ gw_find_udp  │  │   drop_non_  │
        │   _rule()    │  │   udp++      │
        └──────┬───────┘  │   return DROP │
               │          │ else          │
               │          │   route_l2() │
        ┌──────▼───────┐  └──────────────┘
        │ rule found?  │
        └──┬────────┬──┘
           │YES     │NO
           ▼        ▼
    ┌──────────┐ ┌──────────────┐
    │ apply    │ │ drop_no_     │
    │ rewrite  │ │ route++      │
    │ stats    │ │ return DROP  │
    │ forward  │ └──────────────┘
    │ to       │
    │ out_port │
    └──────────┘
```

### 5.2 代码逐段拆解

```c
struct gw_packet_result gw_packet_process(uint16_t in_port,
                                          struct rte_mbuf *m,
                                          const struct gw_config *cfg,
                                          struct gw_runtime_stats *stats)
{
    struct gw_port_stats *ps = &stats->port[in_port];  // 获取入端口统计指针

    // ===== L2: Ethernet 解析 =====
    if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr)) {
        ps->drop_short++;        // 报文太短，连 Ethernet 头都不完整
        return drop_result();
    }

    struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    uint16_t ether_type = rte_be_to_cpu_16(eth->ether_type);  // 网络字节序→主机

    // ===== ARP 分支 =====
    if (ether_type == RTE_ETHER_TYPE_ARP) {
        ps->arp++;
        return route_l2(in_port, eth, cfg, stats);  // L2 转发，只 swap MAC
    }

    // ===== 非 IPv4 分支 =====
    if (ether_type != RTE_ETHER_TYPE_IPV4) {
        ps->non_udp++;
        if (cfg->udp_only) {
            ps->drop_non_udp++;
            return drop_result();      // UDP-only: 丢弃非 IPv4
        }
        return route_l2(in_port, eth, cfg, stats);  // 否则 L2 兜底
    }

    // ===== L3: IPv4 解析 =====
    ps->ipv4++;  // 确认是 IPv4

    struct rte_ipv4_hdr *ip = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
                                                       sizeof(struct rte_ether_hdr));
    const uint8_t ihl = (uint8_t)((ip->version_ihl & 0x0fU) * 4U);  // IHL * 4 = 实际头长
    // 验证 IHL 有效性和报文长度
    if (ihl < sizeof(struct rte_ipv4_hdr) ||
        rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr) + ihl) {
        ps->drop_short++;
        return drop_result();
    }

    // ===== L4: 检查是否为 UDP =====
    if (ip->next_proto_id != IPPROTO_UDP) {
        ps->non_udp++;
        if (cfg->udp_only) {
            ps->drop_non_udp++;
            return drop_result();      // UDP-only: 非 UDP 一律丢弃
        }
        return route_l2(in_port, eth, cfg, stats);
    }

    // ===== UDP 快路径 =====
    ps->udp++;  // 确认是 UDP

    struct rte_udp_hdr *udp = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *,
                                                       sizeof(struct rte_ether_hdr) + ihl);

    // 五元组匹配
    int ri = gw_find_udp_rule(cfg, in_port, ip, udp);
    if (ri < 0) {
        ps->drop_no_route++;  // 无匹配规则
        return drop_result();
    }

    // ===== 命中规则：执行 rewrite =====
    const struct gw_rule *rule = &cfg->rules[ri];
    bool changed = gw_rule_apply_rewrite(rule, eth, ip, udp);

    // 如果规则未指定 MAC rewrite，用 swap_mac 兜底
    if (!gw_rule_has_mac_rewrite(rule))
        maybe_swap_mac(eth, cfg);

    // 更新统计
    ps->rule_hit[ri]++;
    ps->rule_bytes[ri] += rte_pktmbuf_pkt_len(m);
    if (changed) {
        ps->rewrite++;           // 端口级 rewrite 计数
        ps->rule_rewrite[ri]++;  // 规则级 rewrite 计数
    }

    return forward_result(rule->out_port, ri);
}
```

### 5.3 转发/丢弃的决策

```c
static void forward_or_drop(uint16_t in_port, struct rte_mbuf *m,
                            struct gw_packet_result r)
{
    // 情况 1: 无转发决策 或 目标端口不存在 → 丢弃
    if (!r.forward || !port_is_active(r.out_port)) {
        stats.port[in_port].drops++;
        rte_pktmbuf_free(m);    // 归还 mbuf 到 mempool
        return;
    }

    // 情况 2: 转发到目标端口
    // 注意：必须在 tx_burst 之前读取 pkt_len！
    uint32_t pkt_len = rte_pktmbuf_pkt_len(m);

    struct rte_mbuf *tx_pkts[1] = { m };
    uint16_t sent = rte_eth_tx_burst(r.out_port, 0, tx_pkts, 1);

    if (sent == 1) {
        // 发送成功：更新 out_port 的 TX 统计
        stats.port[r.out_port].tx++;
        stats.port[r.out_port].tx_bytes += pkt_len;
    } else {
        // TX ring 满：丢弃
        stats.port[r.out_port].tx_failed++;
        stats.port[in_port].drops++;
        rte_pktmbuf_free(m);
    }
}
```

---

## 6. 规则引擎：匹配与 rewrite

### 6.1 匹配算法

```text
                         gw_find_udp_rule(cfg, in_port, ip, udp)
                                       │
                                       ▼
                        ┌──────────────────────────────┐
                        │  for each rule (0..nb_rules): │
                        │    1. rule enabled?           │
                        │    2. rule.in_port == in_port?│
                        │    3. rule_udp_match()?       │
                        └──────────────┬───────────────┘
                                       │
                    ┌──────────────────┼──────────────────┐
                    │                  │                  │
                    ▼                  ▼                  ▼
            match_src_ip?       match_dst_ip?       match_dst_port?
         ip.src == rule.src_ip  ip.dst == rule.dst_ip  udp.dst == rule.dst_port
                    │                  │                  │
                    └──────────────────┼──────────────────┘
                                       │
                                  ALL match → return rule_index
                                  ANY fail  → continue next rule
                                       │
                                       ▼
                                  no rule matched
                                  return -1 (→ drop_no_route)
```

核心函数：

```c
static bool rule_udp_match(const struct gw_rule *rule,
                           const struct rte_ipv4_hdr *ip,
                           const struct rte_udp_hdr *udp)
{
    // 只有被设置的字段才参与匹配（wildcard 语义）
    if (rule->match_src_ip && ip->src_addr != rule->src_ip)
        return false;
    if (rule->match_dst_ip && ip->dst_addr != rule->dst_ip)
        return false;
    if (rule->match_src_port && udp->src_port != rule->src_port)
        return false;
    if (rule->match_dst_port && udp->dst_port != rule->dst_port)
        return false;
    return true;  // 所有已设置的字段都匹配
}

int gw_find_udp_rule(const struct gw_config *cfg, uint16_t in_port,
                     const struct rte_ipv4_hdr *ip, const struct rte_udp_hdr *udp)
{
    for (uint16_t i = 0; i < cfg->nb_rules && i < GW_MAX_RULES; i++) {
        const struct gw_rule *rule = &cfg->rules[i];
        if (!rule->enabled)       continue;   // 规则未启用
        if (rule->in_port != in_port) continue;  // 方向不匹配
        if (rule_udp_match(rule, ip, udp))
            return i;             // 第一个匹配的规则命中
    }
    return -1;  // 无匹配，触发 drop_no_route
}
```

### 6.2 Rewrite 机制

```text
                         gw_rule_apply_rewrite(rule, eth, ip, udp)
                                       │
                                       ▼
                        ┌──────────────────────────────┐
                        │  遍历 6 个可改写字段:         │
                        │                              │
                        │  set_src_mac → eth.src = val │
                        │  set_dst_mac → eth.dst = val │
                        │  set_src_ip  → ip.src  = val │
                        │  set_dst_ip  → ip.dst  = val │
                        │  set_src_port→ udp.src = val │
                        │  set_dst_port→ udp.dst = val │
                        └──────────────┬───────────────┘
                                       │
                                ┌──────▼──────┐
                                │ IP 改写？    │
                                └──┬──────┬───┘
                                  YES     NO
                                   │       │
                                   ▼       │
                            ┌────────────┐ │
                            │ hdr_checksum│ │
                            │ = 0        │ │
                            │ = rte_ipv4 │ │
                            │   _cksum() │ │
                            └──────┬─────┘ │
                                   │       │
                            ┌──────▼───────▼──┐
                            │ IP 或 UDP 改写？ ├──YES──→ udp.dgram_cksum = 0
                            └─────────────────┘         (让网卡/下游重新计算)
```

关键实现细节：
- **IP 校验和重算**：`rte_ipv4_cksum()` 是 DPDK 提供的快速校验和函数
- **UDP 校验和置零**：改写 IP/Port 后 UDP 校验和失效，置零让接收方不校验（IPv4 合法）
- **MAC swap 兜底**：当规则没有显式 rewrite MAC 时，交换 src/dst MAC 让回复报文能路由回去

---

## 7. 内存模型与 mbuf 生命周期

### 7.1 mbuf 状态机

```text
                   rte_pktmbuf_pool_create()
                           │
                           ▼
              ┌────────────────────────┐
              │   mempool (空闲池)      │  ← 8192 个 mbuf 等待分配
              └───────────┬────────────┘
                          │
              ┌───────────▼────────────┐
              │ NIC DMA → mbuf (填充)   │  ← 网卡收到报文，DMA 到 mbuf
              │ 状态: RX ring 中        │
              └───────────┬────────────┘
                          │
              ┌───────────▼────────────┐
              │ rx_burst() 取出 mbuf    │  ← 应用拿到 mbuf 指针
              │ 状态: 应用持有          │
              └───────────┬────────────┘
                          │
            ┌─────────────┼─────────────┐
            │             │             │
            ▼             ▼             ▼
    ┌───────────┐  ┌───────────┐  ┌───────────┐
    │ tx_burst  │  │ 丢弃 (drop)│  │ 转发到     │
    │ 成功      │  │           │  │ 另一端口    │
    │ PMD 接管  │  │pktmbuf_free│  │ (同上流程)  │
    └─────┬─────┘  └─────┬─────┘  └───────────┘
          │              │
          ▼              ▼
    ┌───────────┐  ┌───────────┐
    │ NIC 发送   │  │ 归还到     │
    │ 后自动回收  │  │ mempool   │
    │ (或完成队列)│  └───────────┘
    └───────────┘
```

### 7.2 关键 Bug：tx_burst 后访问 mbuf

这是一个深刻教训：

```c
// ===== 错误代码 =====
uint16_t sent = rte_eth_tx_burst(port, 0, &m, 1);
if (sent == 1) {
    // BUG! tx_burst 成功后 PMD 拥有 mbuf 所有权
    // 此时访问 m 是 use-after-transfer
    uint32_t len = rte_pktmbuf_pkt_len(m);  // ← 未定义行为
}

// ===== 正确代码 =====
uint32_t pkt_len = rte_pktmbuf_pkt_len(m);  // 先读
struct rte_mbuf *tx_pkts[1] = { m };
uint16_t sent = rte_eth_tx_burst(port, 0, tx_pkts, 1);
if (sent == 1) {
    stats.port[port].tx_bytes += pkt_len;  // 用之前读的值
}
```

**原理**：`rte_eth_tx_burst()` 成功后，mbuf 被 PMD 接管并关联到 TX descriptor。NIC 可能继续 DMA 读取该 buffer；TX 完成后由 PMD 回收，最终返回 mempool，之后才可能被重新用于其他 RX/TX 工作。应用在所有权移交后继续访问属于竞争或 use-after-free 风险。

类比 C++ 的 `std::move`：移动后不应再访问源对象。mbuf 的所有权转移同理。

---

## 8. 统计系统设计

### 8.1 两层统计架构

```text
┌─────────────────────────────────────────────────────────┐
│                    gw_runtime_stats                      │
│  (软件统计 — 应用层计数，每个报文经过时递增)              │
│                                                         │
│  port[0]:                     port[1]:                  │
│  ┌─────────────────────┐     ┌─────────────────────┐    │
│  │ rx          rx_bytes│     │ rx          rx_bytes│    │
│  │ tx          tx_bytes│     │ tx          tx_bytes│    │
│  │ tx_failed   drops   │     │ tx_failed   drops   │    │
│  │ arp  ipv4   udp     │     │ arp  ipv4   udp     │    │
│  │ non_udp     rewrite │     │ non_udp     rewrite │    │
│  │ drop_short           │     │ drop_short           │    │
│  │ drop_non_udp          │     │ drop_non_udp          │    │
│  │ drop_no_route         │     │ drop_no_route         │    │
│  │ rule_hit[0..3]        │     │ rule_hit[0..3]        │    │
│  │ rule_bytes[0..3]      │     │ rule_bytes[0..3]      │    │
│  │ rule_rewrite[0..3]    │     │ rule_rewrite[0..3]    │    │
│  └─────────────────────┘     └─────────────────────┘    │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                   rte_eth_stats                          │
│  (硬件统计 — PMD/NIC 驱动层计数，反映硬件真实收发)        │
│                                                         │
│  port 0: ipackets, ibytes, opackets, obytes             │
│          imissed (RX ring 溢出), ierrors, oerrors        │
│  port 1: ...                                            │
└─────────────────────────────────────────────────────────┘
```

两个统计体系互补：
- **软件统计**：知道应用层"看到了什么"——协议分布、规则命中、丢弃原因
- **硬件统计**：知道网卡层"实际发生了什么"——硬件丢包、队列溢出

### 8.2 累计值 vs 增量值

统计按秒级周期打印，但打印的是**累计值**（不是增量）。这是设计选择：累计值在输出被截断或丢失中间行时仍可还原最终状态。

解析器处理方式（`parse_gateway_stats.py`）：
```python
# 对每个 port，取最后一次采样的值（不是求和）
port_last[port_id] = {...}  # 后来的采样覆盖前面的

# 对 rule_hit，需要按 (port_id, rule_id) 追踪（因为同一 rule_id
# 在不同 port 下是独立的计数器）
rule_hit_last[(current_port, rule_id)] = int(r.group("hit"))
```

---

## 9. 测试策略与 pcap PMD 方案

### 9.1 测试分层

```text
          ┌─────────────────────────────────┐
          │  L3: 真实物理网卡 + 外部发包     │ ← 最接近生产
          │  vmxnet3 + 外部 VM              │
          ├─────────────────────────────────┤
          │  L2: vhost/virtio-user          │ ← 需要 testpmd
          │  testpmd txonly → virtio-user   │
          ├─────────────────────────────────┤
        ★ │  L1: pcap PMD (推荐)            │ ← 无需硬件/无需 sudo
          │  pcap → net_pcap rx → null tx   │
          ├─────────────────────────────────┤
          │  L0: vdev null pair smoke       │ ← 仅验证程序启动
          │  net_null0 ↔ net_null1          │
          └─────────────────────────────────┘
```

### 9.2 pcap PMD 拓扑详解

```text
┌──────────────────────────────────────────────────────────────┐
│                    测试环境 (单机，--no-huge)                  │
│                                                              │
│  ┌─────────────────┐        ┌──────────────────────────┐     │
│  │ gen_udp_pcap.py │───────→│     /tmp/udp_test.pcap    │     │
│  │ (Python stdlib) │        │ 500 UDP pkts, 77 bytes   │     │
│  └─────────────────┘        │ dst_port=9000            │     │
│                             └───────────┬──────────────┘     │
│                                         │                    │
│                              ┌──────────▼──────────┐         │
│                              │  net_pcap0 (port 0)  │         │
│                              │  rx_pcap + infinite  │         │
│                              │  replay            │         │
│                              └──────────┬──────────┘         │
│                                         │ rx_burst → mbuf    │
│                              ┌──────────▼──────────┐         │
│                              │ media-gateway-lite   │         │
│                              │                      │         │
│                              │ Ethernet→IPv4→UDP   │         │
│                              │ rule match (port 9000)│        │
│                              │ rewrite (IP/MAC/Port) │        │
│                              └──────────┬──────────┘         │
│                                         │ tx_burst           │
│                              ┌──────────▼──────────┐         │
│                              │  net_null0 (port 1)  │         │
│                              │  tx accept + discard │         │
│                              └─────────────────────┘         │
└──────────────────────────────────────────────────────────────┘
```

### 9.3 pcap 文件结构

```text
pcap 文件格式:
┌──────────────────────────────────────┐
│ Global Header (24 bytes)             │
│  magic=0xa1b2c3d4                    │
│  version=2.4                         │
│  linktype=Ethernet                   │
├──────────────────────────────────────┤
│ Packet Record 1 (16 + N bytes)       │
│  ┌─ Record Header (16 bytes) ────┐   │
│  │ ts_sec, ts_usec, incl_len     │   │
│  └───────────────────────────────┘   │
│  ┌─ Ethernet Frame (77 bytes) ───┐   │
│  │ dst_mac: 52:54:00:00:00:01    │   │
│  │ src_mac: 52:54:00:00:00:aa    │   │
│  │ EtherType: 0x0800 (IPv4)      │   │
│  │ ┌─ IPv4 Header (20 bytes) ─┐  │   │
│  │ │ src=10.10.10.1            │  │   │
│  │ │ dst=10.10.10.10           │  │   │
│  │ │ protocol=17 (UDP)         │  │   │
│  │ └───────────────────────────┘  │   │
│  │ ┌─ UDP Header (8 bytes) ───┐  │   │
│  │ │ src_port=12345            │  │   │
│  │ │ dst_port=9000             │  │   │
│  │ └───────────────────────────┘  │   │
│  │ ┌─ Payload (35 bytes) ────┐   │   │
│  │ │ HELLO_UDP_MEDIA_GATEWAY_ │   │   │
│  │ │ TEST_PACKET              │   │   │
│  │ └──────────────────────────┘   │   │
│  └───────────────────────────────┘   │
├──────────────────────────────────────┤
│ Packet Record 2 ... 500             │
└──────────────────────────────────────┘
```

### 9.4 为什么选择 pcap PMD

| 方案 | 需要 sudo | 需要物理网卡 | 需要外部机器 | 可复现性 |
|------|----------|-------------|-------------|---------|
| pcap PMD | 不需要 (`--no-huge`) | 不需要 | 不需要 | **极高** |
| vhost-user | 不需要 | 不需要 | testpmd 进程 | 中 |
| 物理 vmxnet3 | 需要 (绑驱动) | 需要 | 需要发包 | 低 |

pcap PMD 的核心优势：任何有 DPDK 的机器都能跑，完全自包含，且能覆盖所有代码路径。

---

## 10. 关键踩坑记录

### Pitfall 1: tx_burst 后访问 mbuf

**现象**：程序能跑，但 valgrind/ASAN 会报 use-after-free，生产环境可能导致数据竞争。

**根因**：`rte_eth_tx_burst()` 成功返回后，mbuf 所有权已移交 PMD，任何后续访问都是未定义行为。

**修复**：在 `tx_burst()` 之前保存需要的数据。

### Pitfall 2: 累计统计的解析

**现象**：python 解析器把同一 rule_id 在不同 port 下的值覆盖了，导致 rule_hit=0。

**根因**：统计输出中，每条规则在每个 port 下都出现一次。解析器把 rule_id 视为全局 ID，但实际是 `(port_id, rule_id)` 组合键。

**修复**：跟踪当前的 `port_id` 上下文，使用 `(port_id, rule_id)` 作为键。

### Pitfall 3: net_null 默认 copy=1

**现象**：`net_null` 端口出现大量 rx，与预期不符。

**根因**：DPDK 21.11 的 net_null PMD 默认 `copy=1`，会把 TX 包复制到 RX 路径。

**影响**：不影响测试结果——这些额外 rx 因为没有匹配规则而被 `drop_no_route` 计数，验证了丢弃路径的正确性。`rte_eth_stats` 中 port 1 的 `ibytes=0` 可以区分真实硬件 RX 和软件 copy。

### Pitfall 4: --no-huge 与 hugepage 的关系

**现象**：测试机没有配 hugepages，启动即报 `EAL: cannot allocate memory`。

**解决**：vdev 测试不需要 hugepages 的性能优势，加 `--no-huge` 使用标准 `malloc` 即可。

---

## 附录 A: 文件清单

```text
project-dpdk-media-gateway-lite/
├── app/                         # 源代码 (~800 行 C)
│   ├── main.c                   # 入口 + 主循环 + 转发逻辑
│   ├── gateway_config.{c,h}     # 配置解析 + 默认值
│   ├── gateway_port.{c,h}       # ethdev 端口管理
│   ├── gateway_packet.{c,h}     # 报文分类处理（核心）
│   ├── gateway_rule.{c,h}       # 规则匹配 + rewrite
│   ├── gateway_stats.{c,h}      # 软件统计 + ethdev stats
│   └── gateway_common.h         # 常量定义
├── scripts/                     # 测试脚本
│   ├── 01_build_app.sh          # 编译
│   ├── 05_run_vdev_null_pair_smoke.sh  # L0 smoke
│   ├── 06_run_pcap_rx_test.sh   # L1 真实流量 ★
│   └── common.sh                # 共享变量/函数
├── tools/
│   ├── gen_udp_pcap.py          # pcap 生成器
│   └── parse_gateway_stats.py   # 统计解析器
├── docs/                        # 文档
│   ├── 01_DEEP_LEARNING.md      # 本文档
│   ├── 02_TEST_AND_VERIFY.md    # 测试指南
│   └── NEXT_STEPS.md            # 状态跟踪
└── records/                     # 测试记录
    └── 20260607-pcap-traffic-test/
```

## 附录 B: 快速复现命令

```bash
# 1. 编译
cd project-dpdk-media-gateway-lite
./scripts/01_build_app.sh

# 2. 生成测试流量
python3 tools/gen_udp_pcap.py /tmp/udp_test.pcap 500

# 3. 运行（无需 sudo，无需物理网卡）
./app/build/media-gateway-lite \
  -l 0-1 -n 4 --no-huge --no-pci \
  --vdev 'net_pcap0,rx_pcap=/tmp/udp_test.pcap,infinite_rx=1' \
  --vdev net_null0 \
  -- \
  --run-seconds 10 --stats-period 2 \
  --promisc 1 --udp-only 1 --swap-mac 0 --strict-rules 0 \
  --rule0 0:1 --rule0-dst-port 9000 \
  --rule0-rewrite-dst-ip 10.10.20.20 \
  --rule0-rewrite-dst-mac 52:54:00:00:00:02 \
  --rule0-rewrite-dst-port 10000

# 4. 查看结果
python3 tools/parse_gateway_stats.py <logfile>
# 预期: PASS_SMOKE=YES / PASS_TRAFFIC=YES / PASS_FORWARDING=YES / PASS_REWRITE=YES
```
