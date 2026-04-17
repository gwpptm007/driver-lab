# stage03_napi_poll

## 阶段定位

NAPI / poll / 中断抑制与可观测性。

## 这一阶段要解决什么问题

stage02 已经证明：
- `ndo_start_xmit()` 可以收到真实发送的帧
- 驱动可以把一帧重新送回协议栈 RX 路径
- 用户态 sender / receiver 可以形成教学型 software loopback

但 stage02 仍然是"**每来一帧就立刻处理一帧**"的思路。它能讲清 `skb`，却讲不清：

- 为什么高频收包不能每包都直接走中断 / 立刻注入
- 为什么 RX 路径需要一个"先缓存、后批处理"的模型
- `budget` 到底限制了什么
- poll 结束时为什么要重新开中断
- 同样是软件环回，`direct` 和 `napi` 两种模式到底差在哪

stage03 的目标：

> **把 stage02 的"立刻注入 RX"改成"先进入 pending queue，再由 NAPI poll 批量处理"。**

## 当前阶段边界

这一阶段**仍然不引入真实 descriptor ring / DMA / RX replenishment**。

先用 `sk_buff_head pending_rxq` 代替真实硬件 ring，stage04 再讨论 ring / DMA / ownership / refill。

## 本阶段核心产出

- 一个教学型 NAPI 驱动：`driver/netdev_stage03.c`
- 两种 RX 模式：`rx_mode=direct` / `rx_mode=napi`
- 两个用户态工具：`send_stage03_frame` / `recv_stage03_frame`
- debugfs 统计：irq / schedule / poll / budget / queue depth

## 核心文档

- [START_HERE.md](START_HERE.md) — 阅读顺序和快速开始
- [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) — 目标与边界
- [docs/02_USER_GUIDE.md](docs/02_USER_GUIDE.md) — 使用指南
- [docs/03_ACCEPTANCE.md](docs/03_ACCEPTANCE.md) — 验收标准
- [docs/04_DEEP_LEARNING.md](docs/04_DEEP_LEARNING.md) — 深度原理
