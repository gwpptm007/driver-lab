# SUMMARY

## 本轮目标
对 e1000e 驱动源码进行第一轮扫描，导出函数索引和关键路径，与 virtio_net 做结构对照。

## 当前已确认的骨架

**e1000e 驱动结构**:
- `e1000_probe()` (line 7383) — PCI probe，alloc_etherdev → register_netdev
- `e1000_remove()` (line 7737) — PCI remove，unregister_netdev
- `struct e1000_adapter` (e1000.h:188) — 驱动私有结构，包含 tx_ring/rx_ring/napi
- `e1000e_poll()` (line 2668) — NAPI poll 回调
- `e1000_clean_tx_irq()` (line 1212) — TX ring 清理
- `e1000_clean_rx_irq()` (line 911) — RX ring 清理（多个变体）
- MSI-X / Legacy 中断支持，per-queue IRQ 配置

**源码规模**:
- `netdev.c`: 7985 行
- `e1000.h`: struct e1000_adapter 定义 + hw 结构
- 辅助芯片文件: ich8lan.c, 82571.c, mac.c, phy.c 等

## 当前和 virtio_net 的最明显差异

| 维度 | virtio_net | e1000e |
|------|-------------|---------|
| 设备模型 | virtio bus 半虚拟化 | PCI 传统 Intel NIC |
| 队列管理 | virtqueue (与 host 共享) | descriptor ring (本地 DMA) |
| RX buffer | page_pool 分配 + build_skb | skb + DMA buffer |
| NAPI | 每个 rx_queue 一个 napi_struct | 整个 adapter 一个 napi |
| 事件通知 | virtqueue kick/callback | MSI-X IRQ + NAPI poll |
| 队列数 | 1 对 (TX+RX) 默认 | 多队列支持 (MSI-X) |

## 当前和自己 netdev stage 的最强映射

- `stage03_napi_poll` → `e1000e_poll()` 函数结构相同
- `stage04_ring_dma` → `e1000e_setup_tx/rx_resources()` ring 分配
- `stage09_multi_queue` → 多 TX/RX ring 支持
- `stage10_msix_per_queue_irq` → MSI-X 中断配置 (`e1000_request_msix()`)
- `stage12_ethtool_control_plane` → ethtool.c 实现了 stats 接口

## 当前还不够的地方

- 还没有深入分析 TX/RX descriptor 格式
- 还没有对比 e1000 vs e1000e 的差异（老版 vs 新版）
- 还没有观察实际运行时的 NAPI 行为
- 还没有完成 stage vs e1000e 的逐项映射

## 下一步

P3: 完成第一轮骨架阅读
- 深挖 `e1000_probe()` 的完整流程（设备初始化 → 队列分配 → 中断注册）
- 分析 `e1000_clean_rx_irq()` 的 RX buffer 补充路径
- 分析 `e1000_clean_tx_irq()` 的 TX 完成回收路径

P4: 完成 virtio_net vs e1000e 对照
- 填写 `virtio_vs_e1000e_matrix.md` 的每个维度结论

P5: 完成 stage vs e1000e 映射
- 逐 stage 对照，确认哪些 stage 概念可以直接映射