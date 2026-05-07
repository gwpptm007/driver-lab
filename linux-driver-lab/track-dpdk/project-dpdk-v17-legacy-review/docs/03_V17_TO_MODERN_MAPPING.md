# 03_V17_TO_MODERN_MAPPING

## 总体对照

| 维度 | DPDK v17 项目经验 | 当前 modern track |
|---|---|---|
| 构建方式 | 传统 make / RTE_SDK / RTE_TARGET | meson/ninja 或 pkg-config libdpdk |
| 设备绑定 | igb_uio / uio_pci_generic / vfio-pci | 当前测试机使用 uio_pci_generic |
| NIC PMD | 物理/虚拟 PMD | vmxnet3 PMD、net_vhost、virtio-user、net_null |
| 内存模型 | hugepage + mempool + mbuf | 一致 |
| 收发 API | `rte_eth_rx_burst` / `rte_eth_tx_burst` | 一致 |
| 队列模型 | RXQ/TXQ + lcore 绑定 | 当前 lab 先单队列，后续可多队列 |
| 回内核 | KNI 常见 | 更倾向 tap/virtio/vhost/AF_XDP 等替代设计 |
| 项目形态 | 媒体面转发模块 | media-gateway-lite / fastpath-lite |

## API 层面稳定的核心

虽然 DPDK 版本变化很大，但数据面的几个核心概念稳定：

```text
EAL
hugepage
mempool
mbuf
ethdev
queue setup
rx_burst / tx_burst
stats
```

这也是当前 track 的学习主线。

## 工程方式变化

旧方式常见：

```bash
export RTE_SDK=/path/to/dpdk
export RTE_TARGET=x86_64-native-linuxapp-gcc
make
```

当前项目方式：

```bash
pkg-config --cflags --libs libdpdk
gcc $(pkg-config --cflags libdpdk) ... $(pkg-config --libs libdpdk)
```

或者：

```bash
meson setup build
ninja -C build
```

## 当前 track 如何证明迁移能力

| 当前目录 | 证明点 |
|---|---|
| `lab-vmxnet3-testpmd` | 知道如何准备 hugepage、绑定 DPDK 口、启动 PMD |
| `lab-vhost-user-basic` | 理解 vhost-user backend socket |
| `lab-virtio-user-vhost` | 理解 virtio-user frontend 与 vhost-user 对接 |
| `lab-dpdk-l2-forwarding` | 能写最小 DPDK C 收发程序 |
| `project-user-space-fastpath` | 能写协议分类和 rewrite 框架 |
| `project-dpdk-media-gateway-lite` | 能把数据面拆成配置、端口、包解析、规则、统计模块 |

## 迁移时要关注的风险

```text
1. 旧 API 是否被替换或参数语义变化
2. KNI 是否仍适合当前系统和内核
3. IOMMU / VFIO / UIO 在虚拟机环境下的可用性
4. checksum 更新是否完整
5. tx_burst 成功后 mbuf 所有权是否已经交给 PMD
6. 多队列和 NUMA 绑定是否明确
7. 统计是否是累计值，解析脚本是否重复求和
```
