# stage02_skb_path / START_HERE

## 优先阅读

**[docs/02_USER_GUIDE.md](docs/02_USER_GUIDE.md)** — 本阶段使用指南

## 阅读顺序

1. `docs/01_STAGE_OVERVIEW.md` — 阶段目标与边界
2. `docs/02_USER_GUIDE.md` — 使用指南（首先读这个）
3. `docs/03_ACCEPTANCE.md` — 验收标准
4. `docs/04_DEEP_LEARNING.md` — 深度原理
5. `driver/netdev_stage02.c`

## 一句话理解 stage02

stage01 解决的是"最小网卡骨架有没有"；stage02 解决的是"网络包对象 `skb` 怎么经过 TX，再被软件方式送回 RX"。

## 本阶段最该理解的 4 件事

1. `skb` 是网络驱动真正处理的核心对象
2. `ndo_start_xmit()` 拿到的是"已经进入驱动发送路径的 skb"
3. 软件环回不是"真实网卡收包"，而是**教学方式重新注入 RX**
4. NAPI / ring / DMA 还没上场，这一阶段先把对象语义吃透

## 快速执行

```bash
cd linux-driver-lab/netdev/stage02_skb_path
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```
