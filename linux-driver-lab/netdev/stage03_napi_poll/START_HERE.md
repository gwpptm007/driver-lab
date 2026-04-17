# stage03_napi_poll / START_HERE

## 优先阅读

**[docs/02_USER_GUIDE.md](docs/02_USER_GUIDE.md)** — 本阶段使用指南

## 阅读顺序

1. `docs/01_STAGE_OVERVIEW.md` — 阶段目标与边界
2. `docs/02_USER_GUIDE.md` — 使用指南（首先读这个）
3. `docs/03_ACCEPTANCE.md` — 验收标准
4. `docs/04_DEEP_LEARNING.md` — 深度原理
5. `driver/netdev_stage03.c`

## 这阶段最重要的一句话

> stage03 不是"学会调用 napi API"，而是学会：
> **为什么要把 RX 处理从"每包立刻处理"切到"先排队、后 poll 批处理"。**

## 快速执行

```bash
cd linux-driver-lab/netdev/stage03_napi_poll
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```
