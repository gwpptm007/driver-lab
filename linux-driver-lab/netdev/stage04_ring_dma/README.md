# stage04_ring_dma

## 阶段定位

这是 netdev 主线里最接近"真实驱动心智模型"的阶段：

- 不再只停留在 `skb`/`NAPI` 教学闭环
- 开始引入 **descriptor / ring / ownership / completion**
- 引入 **streaming DMA map/unmap** 的教学型实现
- 把 **RX replenishment** 当成独立核心主题做透

## 本阶段不追求什么

- 不追求真实物理网卡吞吐
- 不追求复杂 offload / 多队列
- 不追求一下子对齐 `virtio-net` 全部能力

本阶段追求的是：**先把 ring + DMA + refill 这套心智模型建立起来**。

## 当前实现摘要

当前 `driver/netdev_stage04.c` 已经落下：

- 一个教学型 `net_device` 驱动 `nds4`
- TX ring：用 descriptor 模拟"设备读取待发 buffer"
- RX ring：预投递 buffer，设备写入后交还给 CPU
- `dma_map_single()` / `dma_unmap_single()` 教学型路径
- NAPI poll drain RX done descriptors
- 每处理完一个 RX descriptor，立即做 **replenishment**
- `debugfs` 统计与 ring 状态导出

## 核心文档

- [START_HERE.md](START_HERE.md) — 阅读顺序和快速开始
- [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) — 目标与边界
- [docs/02_USER_GUIDE.md](docs/02_USER_GUIDE.md) — 使用指南
- [docs/03_ACCEPTANCE.md](docs/03_ACCEPTANCE.md) — 验收标准
- [docs/04_DEEP_LEARNING.md](docs/04_DEEP_LEARNING.md) — 深度原理
