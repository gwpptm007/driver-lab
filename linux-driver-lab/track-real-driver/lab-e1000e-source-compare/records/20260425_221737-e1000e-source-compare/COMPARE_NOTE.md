# COMPARE NOTE

## 对照维度

1. 设备模型
2. 驱动骨架 (probe/remove/netdev注册)
3. TX / RX 主路径
4. IRQ / NAPI / 事件推进
5. ethtool / stats / control plane
6. 与自己 netdev stage 的映射

## virtio_net

**设备模型**: virtio bus 半虚拟化设备，virtio_net 是 virtio 驱动

**驱动骨架**:
- `virtnet_probe()` — virtio_driver.probe
- `virtnet_remove()` — virtio_driver.remove
- `struct virtnet_info` — 私有结构，包含 num_queue_pairs/sq/rq

**TX 路径**: `start_xmit()` → `virtqueue_add_outbuf()` → `virtqueue_kick()` → host 消费

**RX 路径**: `virtqueue_get_buf()` → `build_skb()` → `netif_receive_skb()`

**IRQ/NAPI**: 每个 RX queue 一个 `struct napi_struct`，MSI-X per-queue

**队列管理**: virtqueue 与 host 共享，不需要本地 DMA 映射

**ethtool**: `ethtool_ops` 实现 stats 获取

## e1000/e1000e

**设备模型**: PCI 传统 Intel NIC，pci_driver 模式

**驱动骨架**:
- `e1000_probe()` (netdev.c:7383) — pci_driver.probe，alloc_etherdev → register_netdev
- `e1000_remove()` (netdev.c:7737) — pci_driver.remove，unregister_netdev
- `struct e1000_adapter` (e1000.h:188) — 私有结构，包含 tx_ring/rx_ring/napi

**TX 路径**: `e1000e_xmit_frame()` → `e1000_tx_map()` → `e1000_tx_queue()` → 写 TX descriptor → writel(tail)

**RX 路径**: `e1000_clean_rx_irq()` → `e1000_alloc_rx_buffers()` → `netif_receive_skb()`

**IRQ/NAPI**:
- 整个 adapter 一个 `struct napi_struct`
- `e1000e_poll()` (line 2668) 处理 TX/RX cleanup
- MSI-X / Legacy 中断模式

**队列管理**: 本地 descriptor ring，DMA 映射需要软件维护

**ethtool**: `ethtool.c` 实现 (ethtool_ops 扩展)

## 我自己的 netdev/stage00~stage13

| stage | 概念 | e1000e 对应 |
|-------|------|------------|
| stage03 | napi_poll | `e1000e_poll()` 相同结构 |
| stage04 | ring/dma | `e1000e_setup_tx/rx_resources()` ring 分配 |
| stage08 | async backend | NAPI + IRQ 协同处理 |
| stage09 | multi_queue | 多 TX/RX ring (`adapter->tx_ring`, `adapter->rx_ring`) |
| stage10 | msix per-queue | `e1000_request_msix()` MSI-X 配置 |
| stage11 | page_pool rx | RX buffer 分配 (`e1000_alloc_rx_buffers()`) |
| stage12 | ethtool control plane | `e1000e_ethtool.c` stats 接口 |

## 当前结论

**最大差异 — 设备模型**:
- virtio_net: virtio bus，半虚拟化，virtqueue 与 host 共享
- e1000e: PCI 传统网卡，descriptor ring 本地 DMA

**结构差异 — NAPI 模型**:
- virtio_net: per-queue napi (每个 rx_queue 一个 napi_struct)
- e1000e: per-adapter napi (整个驱动一个 napi)

**相似之处 — 核心概念**:
- 都用 NAPI + poll 模式处理 RX
- 都有 TX/RX ring + descriptor
- 都支持 MSI-X 中断
- 都实现 ethtool ops

**e1000 vs e1000e**:
- e1000: 旧版 (8254x 系列)，更老的芯片
- e1000e: 新版 (ICH8/ICH10/82571 等)，更现代的接口