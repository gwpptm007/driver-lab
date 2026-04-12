# netdev

本目录是 `linux-driver-lab` 在 `foundation/day01~day35` 之后的第二阶段主线：**网络驱动（netdev）**。

这条线的核心目标不是“再做一个能发包的 demo”，而是完成一次从：

- 字符设备 / platform / PCIe / DMA

到：

- `net_device`
- `sk_buff`
- NAPI
- RX/TX ring
- RX replenishment
- `virtio-net` 对照理解
- 跨平台迁移与收口

的范式切换。

## 目录定位

- `stage00_bootstrap/`：架构中立的启动与依赖检查骨架
- `stage01_netdev_skeleton/`：最小 `net_device` 骨架
- `stage02_skb_path/`：`skb` 路径与软件收发闭环
- `stage03_napi_poll/`：NAPI / poll / 中断抑制与观测
- `stage04_ring_dma/`：ring / DMA / RX replenishment / 稳定性
- `stage05_virtio_param/`：`virtio-net` 对照 + 平台参数化
- `stage06_arm64_migration/`：ARM64 迁移与跨平台收口

## 当前明确的阶段原则

### 1. Stage01~Stage04 先专注 netdev 本体
前四阶段不把主线学习绑死在 `arm64 + qemu-system-aarch64 + 交叉编译` 上。
先把这几个核心概念吃透：

- `net_device`
- `sk_buff`
- NAPI
- ring / descriptor
- RX replenishment
- 观测方法

### 2. Stage05~Stage06 再做 ARM64 迁移
后两阶段再把：

- `virtio-net` 源码理解
- 平台参数化
- ARM64 迁移
- 跨平台回归

合起来收口，额外形成“平台迁移能力”这一项成果。

### 3. tap 不是第一阶段设备模型答案
`tap` 可以作为后端收发通道，但它不是“学习设备模型”的唯一答案。
本主线优先强调：

- 前端：教学型自研 netdev
- 后端：先软件注入 / 内部环回，再逐步扩展到更真实 transport

## 建议阅读顺序

1. `docs/00_START_HERE.md`
2. `docs/01_DIRECTION_AND_SCOPE.md`
3. `docs/02_MASTER_PLAN_AND_PHASES.md`
4. `docs/03_DEVICE_ROUTE_DECISION.md`
5. `docs/06_ACCEPTANCE_AND_MILESTONES.md`
6. `docs/08_PLATFORM_STRATEGY.md`

## 一句话总结

这条线不是“从 ARM64 开始学网络驱动”，而是：

> 先把 netdev 本体学清楚，再把它迁到 ARM64，并把整个实验做成平台可配置、可观测、可评审的作品线。
