# stage05_virtio_param / START_HERE

## 优先阅读

**[docs/00_USER_GUIDE.md](docs/00_USER_GUIDE.md)** — 本阶段使用指南：
- stage05 是做什么的
- 在测试机上如何执行
- 每一步验证什么、如何拿结果
- 产物与本地文件对应关系
- 原理图：我们学到了什么

## 学习路径（按顺序）

1. `docs/00_USER_GUIDE.md` ← **首先读这个**
2. `docs/01_STAGE_GOAL_AND_BOUNDARY.md` — 阶段目标与边界
3. `docs/02_VIRTIO_NET_READING_MAP.md` — virtio-net 源码阅读地图
4. `docs/03_STAGE04_TO_VIRTIO_COMPARISON.md` — stage04 ↔ virtio-net 对照
5. `docs/04_PLATFORM_PARAMETERIZATION.md` — 平台参数化详解
6. `docs/06_DEEP_LEARNING.md` — 深度原理（RX replenishment / TX / DMA / NAPI / ownership）

## 快速执行

```bash
cd linux-driver-lab/netdev/stage05_virtio_param
make smoke
```
