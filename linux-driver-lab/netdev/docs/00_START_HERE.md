# netdev / START HERE

## 先看什么

先按这个顺序看：

1. `01_DIRECTION_AND_SCOPE.md`
2. `02_MASTER_PLAN_AND_PHASES.md`
3. `03_DEVICE_ROUTE_DECISION.md`
4. `06_ACCEPTANCE_AND_MILESTONES.md`
5. `08_PLATFORM_STRATEGY.md`

## 先回答的三个问题

### Q1：这条线到底在学什么？
不是单纯学“如何注册一个网卡”，而是系统理解：

- `net_device`
- `sk_buff`
- NAPI
- ring / descriptor
- DMA 与 streaming map/unmap
- RX replenishment
- `virtio-net`
- 跨平台迁移

### Q2：为什么不一开始就上 ARM64？
因为 `stage01~stage04` 先专注 netdev 子系统本体，避免一开始被交叉编译、QEMU 架构、BusyBox/rootfs 这些平台问题拖走注意力。

### Q3：为什么不是一开始就直接读 `virtio-net`？
因为先自己做一条教学型 netdev 链，后面再看 `virtio-net`，会更容易分辨：

- 哪些机制是必须的
- 哪些实现是可替换的
- 为什么 ring / NAPI / refill 要这么设计

## 阶段切分

- `stage00`：把项目启动方式、依赖检查、变量化骨架搭起来
- `stage01~stage04`：专注 netdev 本体
- `stage05`：`virtio-net` 对照 + 平台参数化
- `stage06`：ARM64 迁移与跨平台收口
