# stage04_ring_dma / START_HERE

## 先看什么

1. `README.md`
2. `docs/01_STAGE_OVERVIEW.md` — 阶段目标与边界
3. `docs/02_USER_GUIDE.md` — 使用指南
4. `docs/03_ACCEPTANCE.md` — 验收标准
5. `docs/04_DEEP_LEARNING.md` — 深度原理
6. `driver/netdev_stage04.c`

## 一句话理解本阶段

stage03 已经证明了 `NAPI` 这件事"能工作"。

stage04 要回答的是：

> 如果驱动手里真的有一组 RX/TX descriptor ring，并且 RX buffer 需要不断补充，那整条路径该怎样理解？

## 快速执行

```bash
cd linux-driver-lab/netdev/stage04_ring_dma
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```
