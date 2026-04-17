# stage05_virtio_param

## 阶段定位

Stage05 的核心是两件事：

- `virtio-net` 对照学习
- 平台参数化准备

它不新增一套教学网卡实现，而是把 `stage01 ~ stage04` 建立起来的：

- `net_device`
- `skb`
- NAPI
- ring / descriptor
- RX replenishment

映射到真实的成熟实现 `virtio-net` 上；同时把后续 ARM64 迁移需要的平台差异先抽到 env / scripts 层。

## 本阶段产出

- `virtio_net.c` 阅读地图
- `stage04 ↔ virtio-net` 对照报告
- 平台矩阵
- 可 source 的 resolved env
- 阶段总报告

## 一句话总结

> stage05 不是重写驱动，而是把 stage04 的教学坐标系升级成能与 `virtio-net` 对话、并能迁移到多平台的工程化坐标系。

## 核心文档

- [START_HERE.md](START_HERE.md) — 阅读顺序和快速开始
- [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) — 目标与迁移策略
- [docs/02_USER_GUIDE.md](docs/02_USER_GUIDE.md) — 使用指南
- [docs/03_ACCEPTANCE.md](docs/03_ACCEPTANCE.md) — 验收标准
- [docs/04_DEEP_LEARNING.md](docs/04_DEEP_LEARNING.md) — 深度分析
