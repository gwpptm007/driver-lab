# Linux Network Data Plane Final Report

## 1. Project Goal

本项目目标是完成一条 Linux 网络数据面学习与作品化收口路径，覆盖：

```text
kernel netdev
real driver
virtual network
DPDK userspace fastpath
AF_XDP native fastpath
eBPF observability
```

项目重点不是堆 demo，而是围绕同一个问题建立系统理解：

```text
Linux 网络包在不同数据面路径中如何进入、处理、转发、观测和定位？
```

## 2. Capability Map

| 能力层 | 已覆盖内容 |
|--------|------------|
| Kernel netdev | `net_device`, `skb`, NAPI, ring, multi-queue, MSI-X, page_pool, ethtool, offload, XDP |
| Real driver | `virtio_net` source dive, runtime observe, ethtool stats patch, NAPI trace, `e1000e` compare |
| Virtual network | tap, bridge, virtio frontend, vhost backend, kick/notify, two-guest L2 path |
| DPDK | hugepage, PMD, testpmd, vhost-user, virtio-user, l2fwd-lite, fastpath-lite, media-gateway-lite |
| AF_XDP | XDP attach, XDP actions, XSKMAP, AF_XDP socket, UMEM, rings, mini forwarder |
| eBPF observability | RX/GRO/TX/drop stats, drop reason, per-interface, per-CPU, path invariant |

## 3. Kernel Netdev Path

Kernel netdev 是本项目的基准路径。它建立了 Linux 网络驱动的核心模型：

```text
device lifecycle
queue lifecycle
packet lifecycle
NAPI lifecycle
stats/control lifecycle
fastpath hook
```

已完成 `netdev/stage00~stage14`，从最小 netdev skeleton 推进到 XDP basics。

## 4. Real Driver Path

Real driver path 把教学驱动模型迁移到真实驱动源码：

```text
virtio_net source dive
runtime observe
ethtool stats mini patch
napi_poll trace chain
e1000e compare
```

这条路径证明了教学型 netdev 模型可以映射到真实 Linux NIC 驱动代码中。

## 5. Virtual Network Path

Virtual network path 关注 host/guest 协同：

```text
guest virtio frontend
virtqueue
vhost backend
tap
bridge
kick/notify
```

它把 netdev 能力扩展到虚拟化网络和云场景。

## 6. DPDK Fastpath

DPDK path 完成从环境搭建到用户态 fastpath 原型：

```text
hugepage
devbind
PMD
EAL
mempool
rx_burst / tx_burst
UDP classify / forward / rewrite
```

`project-dpdk-media-gateway-lite` 已在 pcap PMD path 下验证：

```text
PASS_TRAFFIC
PASS_FORWARDING
PASS_REWRITE
```

## 7. AF_XDP Path

AF_XDP path 完成 Linux 原生 fastpath 验证：

```text
XDP attach
XDP_PASS / XDP_DROP / XDP_REDIRECT
XSKMAP
AF_XDP socket
UMEM
FILL/RX/TX/COMPLETION rings
mini forwarder
```

已验证 frame 生命周期闭环：

```text
FILL -> RX -> TX -> COMPLETION -> FILL
```

## 8. eBPF Observability

eBPF observability path 提供横向观测能力：

```text
RX
GRO
TX queue
TX xmit
drop reason
per-interface stats
per-CPU distribution
path invariant
```

它让项目从“能转发”推进到“能解释路径事实”。

## 9. Cross-Path Comparison

| 路径 | 数据入口 | 核心机制 | 价值 | 边界 |
|------|----------|----------|------|------|
| Kernel netdev | 驱动 RX | skb/NAPI/ring | Linux 原生主路径 | 协议栈成本较高 |
| Real driver | 真实驱动 | probe/queue/NAPI/TX/RX | 贴近工业代码 | patch 范围需谨慎 |
| Virtual net | guest/host | virtqueue/tap/bridge/vhost | 云和虚拟化相关 | 路径长，定位复杂 |
| DPDK | PMD RX | polling/mbuf/burst | 用户态高性能路径 | 绕过内核，部署复杂 |
| AF_XDP | XDP redirect | XSKMAP/UMEM/rings | Linux 原生用户态 fastpath | zero-copy 依赖 NIC/driver |
| eBPF observe | trace/event | stats/drop/invariant | 定位能力强 | 不负责转发 |

## 10. Evidence Index

证据入口：

- `evidence/netdev_evidence.md`
- `evidence/real_driver_evidence.md`
- `evidence/virtual_net_evidence.md`
- `evidence/dpdk_evidence.md`
- `evidence/af_xdp_evidence.md`
- `evidence/ebpf_observability_evidence.md`

## 11. Limitations

当前项目应定位为实验型 Linux network data plane portfolio。

不要夸大为：

- 生产级 DPDK 媒体网关。
- 完整真实 NIC 大规模压测。
- 云网络控制面产品。
- 生产级可观测性平台。

## 12. Final Conclusion

本项目已经形成从 Linux 内核网络驱动到用户态 fastpath，再到 eBPF 观测定位的完整能力链。

对外推荐表述：

```text
完成 Linux 网络数据面作品集，覆盖 kernel netdev、真实驱动源码、虚拟化网络、DPDK、AF_XDP 与 eBPF observability；具备从驱动模型、真实代码、虚拟化路径、用户态 fastpath 到观测定位的系统理解，并沉淀脚本、records 和报告证据。
```
