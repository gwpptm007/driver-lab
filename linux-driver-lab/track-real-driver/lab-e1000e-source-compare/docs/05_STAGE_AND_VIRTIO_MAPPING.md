# 05_STAGE_AND_VIRTIO_MAPPING

## 一、和自己 `netdev/stage00~stage13` 的映射

| 自己的阶段 | 在 `e1000/e1000e` 里要重点看什么 |
|---|---|
| `stage01_netdev_skeleton` | netdev 分配 / 初始化 / 注册 |
| `stage03_napi_poll` | poll / budget / 事件推进 |
| `stage04_ring_dma` | descriptor / ring / reclaim |
| `stage09_multi_queue_scaling` | 多队列组织（如果适用） |
| `stage10_msix_per_queue_irq` | IRQ / queue / CPU / NAPI 关系 |
| `stage11_page_pool_rx` | RX buffer / recycle 思维对照 |
| `stage12_ethtool_control_plane` | ethtool / stats 暴露 |
| `stage13_offload_basics` | checksum / offload / 能力边界 |

## 二、和 `virtio_net` 的对照

| 维度 | `virtio_net` | `e1000/e1000e` |
|---|---|---|
| 设备模型 | 半虚拟化 virtio | 传统 PCI NIC 驱动 |
| 主线感觉 | 更强调 capability / event model | 更强调传统 queue / register / interrupt 组织 |
| stats / ethtool | 控制面能力边界感强 | 更适合补传统 NIC 控制面视角 |
| 实验价值 | guest/virtio 主线 | 第二真实驱动对照主线 |
