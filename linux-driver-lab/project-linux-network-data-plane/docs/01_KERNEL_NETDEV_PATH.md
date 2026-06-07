# Kernel Netdev Path

## 路径定位

Kernel netdev path 是整个项目的内核网络驱动主线。它承接 foundation 中的字符设备、platform、PCIe、DMA 基本功，进入 Linux 网络子系统的核心模型。

这条路径要回答：

```text
一个教学型网络驱动如何注册为 net_device？
包如何以 skb 形式进入内核协议栈？
NAPI、ring、queue、IRQ、page_pool、offload、XDP 在驱动中分别解决什么问题？
```

## 覆盖范围

对应主线：

```text
linux-driver-lab/netdev/stage00_bootstrap
  -> stage14_xdp_basics
```

已覆盖能力：

| 主题 | 内容 |
|------|------|
| net_device | 最小网络设备注册、open/stop、ndo callbacks |
| skb path | skb 分配、填充、接收、发送路径 |
| NAPI | poll 模型、中断抑制、budget、收包推进 |
| ring model | RX/TX ring、descriptor、replenishment、queue lifecycle |
| multi-queue | 多队列模型、队列分发、扩展性 |
| MSI-X | per-queue IRQ、队列与中断绑定 |
| page_pool | RX page recycle、内存复用 |
| ethtool | stats/control plane |
| offload | checksum/GRO/GSO 基础 |
| XDP | XDP hook、fast path 起点 |

## 阶段价值

这条路径的价值不是做一个能 `ping` 的 demo，而是建立 Linux 网络驱动的基本坐标系：

```text
device lifecycle
  -> queue lifecycle
  -> packet lifecycle
  -> interrupt/NAPI lifecycle
  -> stats/control lifecycle
  -> fastpath hook
```

后续真实驱动、DPDK、AF_XDP 都可以回到这套模型里做对照。

## 和其他路径的关系

| 后续路径 | 依赖的 netdev 概念 |
|----------|-------------------|
| Real driver | `virtio_net` / `e1000e` 中的 probe、queue、NAPI、TX/RX |
| Virtual net | virtio 前端、tap/bridge 与内核收发路径 |
| AF_XDP | XDP attach、XSKMAP redirect、驱动早期 RX hook |
| eBPF observability | RX/GRO/TX/drop trace 与 netdev 事件 |

## Evidence 入口

主要证据索引：

- `../../netdev/README.md`
- `../../netdev/stage14_xdp_basics/records/`
- `../../netdev/stage14_xdp_basics/reports/`
- [../evidence/netdev_evidence.md](../evidence/netdev_evidence.md)

## 当前边界

准确表述：

- 已完成教学型 netdev 主线，从 skeleton 到 XDP 入口。
- 已覆盖网络驱动常见核心机制，并形成阶段化 records。

不要夸大：

- 不是生产级 NIC 驱动。
- 不代表已经完成所有硬件 offload、RSS、队列亲和性和真实 NIC 压测。
