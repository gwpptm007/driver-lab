# Interview Share Script

## 30 秒版本

我做了一个 Linux 网络数据面学习与作品集项目，从内核 netdev 教学驱动开始，覆盖 `net_device`、`skb`、NAPI、ring、多队列、MSI-X、page_pool、ethtool、offload 和 XDP；然后迁移到真实 `virtio_net` / `e1000e` 驱动源码阅读和小 patch；再扩展到 tap/bridge/vhost 虚拟化网络、DPDK 用户态 fastpath、AF_XDP 原生 fastpath，最后用 eBPF 做 RX/TX/drop 路径观测。每个阶段都有脚本、records 和报告证据。

## 3 分钟版本

这个项目的目标是系统理解 Linux 网络数据面，而不是只写一个收发包 demo。

第一阶段我先做 kernel netdev 主线，从最小 `net_device` 开始，逐步补上 `skb` 收发、NAPI poll、RX/TX ring、queue lifecycle、多队列、MSI-X、page_pool、ethtool、offload，最后到 XDP 入口。这个阶段让我建立了 Linux 网络驱动的基本模型。

第二阶段我把这个模型迁移到真实驱动源码里，主要看 `virtio_net` 和 `e1000e`。我做了 `virtio_net` source dive、运行期观测、ethtool stats mini patch、`napi_poll -> netif_receive_skb` trace 链路，以及 `virtio_net` 和 `e1000e` 的对照阅读。

第三阶段我看虚拟化网络路径，包括 tap、bridge、virtio frontend、vhost backend 和 kick/notify。这个阶段关注 guest 和 host 之间的包如何通过 virtqueue、tap 和 bridge 转发。

第四阶段我做 DPDK 用户态 fastpath，从 hugepage、PMD、testpmd、vhost-user/virtio-user，到自写 l2fwd-lite、fastpath-lite 和 media-gateway-lite。media-gateway-lite 已经在 pcap PMD 路径下验证了 UDP traffic、forwarding 和 rewrite。

第五阶段是 AF_XDP，关注 Linux 原生 fastpath。我验证了 XDP attach、PASS/DROP/REDIRECT、XSKMAP、AF_XDP socket、UMEM 和四个 rings，并做了 mini forwarder。

最后我做了 eBPF observability，用来观测 RX、GRO、TX queue、TX xmit、drop reason、per-interface 和 per-CPU 分布。这样整个项目从转发路径到观测定位就形成了闭环。

## 10 分钟展开

### 1. 为什么做这个项目

我希望把 Linux 网络数据面按路径系统梳理清楚：

```text
内核 netdev 主路径
真实驱动源码
虚拟化网络
DPDK 用户态 fastpath
AF_XDP 原生 fastpath
eBPF 可观测性
```

这几条线不是孤立的。它们共同回答一个问题：网络包从设备到内核、从内核到用户态、从 host 到 guest、从 fastpath 到观测，分别怎么走。

### 2. Kernel netdev

我先用教学型 netdev 建立模型。重点不是能不能发包，而是理解：

```text
net_device lifecycle
skb lifecycle
NAPI poll lifecycle
RX/TX ring lifecycle
queue and IRQ lifecycle
stats/control lifecycle
XDP hook
```

这为后续看真实驱动提供了坐标。

### 3. Real driver

真实驱动部分主要看 `virtio_net` 和 `e1000e`。我把 netdev stage 中的概念映射到真实源码：

```text
probe
queue init
TX path
RX path
NAPI poll
ethtool stats
feature/offload
XDP
```

同时做了一个低风险 ethtool stats patch 和 trace 证据链，用 before/after 的方式证明 patch 点和运行期行为。

### 4. Virtual network

虚拟化网络部分关注 host/guest 协同：

```text
guest virtio frontend
virtqueue
vhost backend
tap
bridge
kick/notify
```

这条线让我把单机 netdev 理解扩展到云和虚拟化场景。

### 5. DPDK

DPDK 部分关注用户态 PMD 数据面：

```text
hugepage
devbind
PMD
EAL
mempool
rx_burst / tx_burst
software stats
UDP classify / forward / rewrite
```

最终项目是 media-gateway-lite，在 pcap PMD 路径下做了真实 UDP 输入、转发和 rewrite 验证。

### 6. AF_XDP

AF_XDP 是另一条用户态 fastpath，但它从 Linux 原生 XDP hook 进入：

```text
XDP attach
XDP_REDIRECT
XSKMAP
AF_XDP socket
UMEM
FILL/RX/TX/COMPLETION rings
```

我做到了 mini forwarder，并验证了 TX 和 COMPLETION 非零，形成 frame 生命周期闭环。

### 7. eBPF observability

最后用 eBPF 做观测：

```text
RX
GRO
TX queue
TX xmit
drop reason
per-interface stats
per-CPU distribution
```

这让项目不只停在“能转发”，还可以解释“路径上发生了什么”。

## 常见追问

### DPDK 和 AF_XDP 的差异是什么？

DPDK 是 PMD 用户态轮询模型，通常绕过内核协议栈；AF_XDP 是 Linux 原生 fastpath，从 XDP hook 通过 XSKMAP redirect 到用户态 UMEM/rings。DPDK 生态成熟、控制力强；AF_XDP 更贴近 Linux 原生路径，但 zero-copy 更依赖 NIC 和 driver 支持。

### 为什么需要真实驱动源码阅读？

教学驱动可以建立模型，但真实驱动能验证模型是否成立。比如 queue、NAPI、TX/RX、stats、feature、XDP 在 `virtio_net` 中都有真实落点。读真实驱动还能训练低风险 patch 选点和 before/after 验证。

### eBPF 在这个项目里解决什么问题？

eBPF 负责观测和定位。它可以统计 RX、GRO、TX queue、TX xmit、drop reason、interface 和 CPU 分布，帮助判断包是否按预期经过关键路径。

### 当前项目不能夸大的地方是什么？

不能说是生产级 DPDK 网关，也不能说完成了真实 NIC 大规模性能压测。准确说法是：完成了实验型 Linux 网络数据面能力链，DPDK media-gateway-lite 在 pcap PMD 路径下验证了 traffic/forwarding/rewrite，AF_XDP 完成了 mini forwarder 和 UMEM/rings 生命周期验证。
