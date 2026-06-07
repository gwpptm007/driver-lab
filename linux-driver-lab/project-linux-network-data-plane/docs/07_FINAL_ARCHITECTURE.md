# Final Architecture

## 总体架构

本项目的最终架构不是一个单一程序，而是一套 Linux 网络数据面能力地图：

```text
                         eBPF Observability
                  trace / stats / drop reason / invariant
                                   ^
                                   |
        +--------------------------+--------------------------+
        |                          |                          |
  Real Driver                Kernel Netdev                AF_XDP Path
 virtio/e1000e           skb / NAPI / ring / XDP       XSKMAP / UMEM / rings
        |                          |                          |
        +--------------------------+--------------------------+
                                   |
                                   v
                           Virtual Network
                      tap / bridge / vhost / virtio
                                   |
                                   v
                             DPDK Fastpath
                       PMD / EAL / mbuf / gateway-lite
```

## 分层说明

### 1. Kernel netdev 是基准坐标系

`net_device`、`skb`、NAPI、ring、queue、IRQ、page_pool、ethtool、offload、XDP 构成 Linux 网络驱动的主路径模型。

后续所有路径都可以回到这里做对照。

### 2. Real driver 验证模型是否能迁移

`virtio_net` 和 `e1000e` 用来证明教学驱动模型不是孤立 demo，而是能映射到真实驱动源码：

```text
probe
queue setup
RX/TX
NAPI poll
stats
feature/offload
XDP
```

### 3. Virtual network 扩展到 host/guest 场景

虚拟化路径把单机 netdev 扩展成：

```text
guest virtio frontend
  -> virtqueue
  -> vhost/tap backend
  -> host bridge
  -> peer guest / host network
```

这条路径连接内核网络和云/虚拟化网络场景。

### 4. DPDK 和 AF_XDP 是两条 fastpath

DPDK 和 AF_XDP 都把数据面推向用户态，但入口和生态不同：

```text
DPDK:
  NIC/PMD -> EAL -> mbuf -> rx_burst/tx_burst -> userspace gateway

AF_XDP:
  driver XDP hook -> XSKMAP -> AF_XDP socket -> UMEM/rings -> userspace forwarder
```

### 5. eBPF observability 是横切层

eBPF observability 不替代转发路径，而是横向提供：

```text
RX event
GRO event
TX queue event
TX xmit event
drop reason
per-interface stats
per-CPU distribution
path invariant
```

## 横向对比

| 路径 | 包入口 | 核心机制 | 优势 | 边界 |
|------|--------|----------|------|------|
| Kernel netdev | 驱动 RX | skb/NAPI/ring | Linux 原生主路径 | 协议栈成本较高 |
| Real driver | 真实 NIC/virtio 驱动 | probe/queue/NAPI/TX/RX | 贴近工业代码 | patch 范围需谨慎 |
| Virtual net | guest/host virtio/tap | virtqueue/vhost/bridge | 云和虚拟化相关 | 路径长，定位复杂 |
| DPDK | PMD RX | polling/mbuf/burst | 用户态高性能、控制力强 | 绕过内核，部署复杂 |
| AF_XDP | XDP redirect | XSKMAP/UMEM/rings | Linux 原生 fastpath | zero-copy 依赖 NIC/driver |
| eBPF observe | tracepoints/kprobes | event/stats/drop | 定位能力强 | 不负责转发 |

## 一条包的视角

从包的视角，可以这样理解：

```text
1. 在普通内核路径中，包进入驱动 RX，然后变成 skb，经 NAPI 进入协议栈。
2. 在真实驱动中，这个过程落到 virtio_net/e1000e 的 queue、ring、poll、stats 实现里。
3. 在虚拟化路径中，包还要经过 guest frontend、virtqueue、vhost/tap 和 host bridge。
4. 在 DPDK 中，包绕过内核协议栈，由 PMD 和用户态循环直接处理。
5. 在 AF_XDP 中，包从 XDP hook redirect 到用户态 UMEM/rings。
6. 在观测层中，eBPF 负责回答包在 RX、GRO、TX、drop 节点上的事实。
```

## 最终结论

这个项目证明的不是单点功能，而是一套网络数据面系统理解：

```text
能从 Linux 内核网络驱动主路径出发，
读懂真实驱动，
解释虚拟化网络，
实现用户态 fastpath 原型，
验证 AF_XDP 原生 fastpath，
并用 eBPF 做路径观测和问题定位。
```
