# stage02_skb_path

## 阶段定位

`skb` 路径与软件 TX/RX 闭环。

## 这一阶段要解决什么问题

stage01 已经证明：最小 `net_device` 能注册/注销，`ndo_open/stop/start_xmit` 能被真实触发。

stage02 往前走一步，不再满足于"看到包到了 TX 入口"，而是要把下面这件事做成闭环：

> **把一帧从 TX 路径拿到手，再以软件教学方式重新注入 RX 路径，形成最小的软件环回。**

## 当前阶段边界

这一阶段**先不引入 NAPI、ring、DMA、RX replenishment**。

- 先理解 `skb` 是什么
- 先理解 TX / RX 在驱动里的入口出口
- 先理解 `netif_rx()` 把包重新交还给协议栈意味着什么
- 再到 stage03/stage04 讨论批处理、descriptor、DMA 搬运

## 本阶段核心产出

- 一个教学型 `net_device` 模块：`driver/netdev_stage02.c`
- 软件环回 TX/RX 闭环
- 两个用户态工具：`send_stage02_frame` / `recv_stage02_frame`
- 解释 `skb clone/copy` 的文档与调试统计

## 核心文档

- [START_HERE.md](START_HERE.md) — 阅读顺序和快速开始
- [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) — 目标与边界
- [docs/02_USER_GUIDE.md](docs/02_USER_GUIDE.md) — 使用指南
- [docs/03_ACCEPTANCE.md](docs/03_ACCEPTANCE.md) — 验收标准
- [docs/04_DEEP_LEARNING.md](docs/04_DEEP_LEARNING.md) — 深度原理
