# netdev / START HERE

## 先看什么

1. `01_START_HERE.md` — 本文档（入口导航 + 设备路线）
2. `02_DIRECTION_AND_PLAN.md` — 学习方向、总体计划、阶段任务拆解
3. `03_ARCHITECTURE_AND_PLATFORM.md` — 南北向架构 + 平台策略
4. `04_MILESTONES_AND_RISKS.md` — 里程碑(M0-M6) + 可观测性 + 风险与Gate
5. `05_STAGE06_STAGE07_PLAN.md` — stage06 收口 + stage07 执行计划

---

## 先回答的三个问题

### Q1：这条线到底在学什么？
不是单纯学"如何注册一个网卡"，而是系统理解：

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

---

## 阶段切分

- `stage00`：把项目启动方式、依赖检查、变量化骨架搭起来
- `stage01~stage04`：专注 netdev 本体
- `stage05`：`virtio-net` 对照 + 平台参数化
- `stage06`：ARM64 迁移与跨平台收口

---

## 设备路线选择说明

### 三个核心概念

**1. 前端设备模型**
guest 驱动面对的对象是什么：

- 教学型自研 netdev
- 简化 MMIO/shared-ring 网卡
- `virtio-net`

**2. 后端数据通道**
数据从哪里来、到哪里去：

- 软件注入
- 内部环回
- 用户态工具
- tap/tun
- shared memory

**3. 外部联通方式**
是否接真网络：

- 不接外网
- 只做本地收发
- 接 tap / bridge / NAT

---

### 推荐路线

**Stage01~Stage04** 优先走：

- **前端：教学型自研 netdev**
- **后端：先软件注入 / 内部环回**

原因：
- 能把注意力放在 `net_device` / `skb` / NAPI / ring 上
- 不会一开始就被 virtio feature、tap 配置、QEMU 网络联通细节拖住
- 更适合做可解释、可截图、可归档的教学实验

**Stage05** 引入：
- `virtio-net` 源码对照
- 必要的平台参数化
- 可选 tap 路线说明

**Stage06** 再看：
- ARM64 QEMU
- 跨平台运行脚本
- 差异对比

---

### 为什么不把 tap 当成第一阶段答案

tap 很有用，但它回答的是"包怎么进出宿主机"，不是"你在 guest 里学到的设备模型是什么"。

tap 可以用，但不能替代"前端设备模型设计"的思考。

---

### 为什么不直接上 virtio-net

因为一上来就会混入：

- virtqueue / vring
- feature negotiation
- backend 细节
- 更复杂的 buffer 管理

这会让第一阶段的学习目标变得不纯。

---

## 当前下一步建议

如果你是按当前最新完整项目继续推进，建议优先读：

- `stage06_arm64_migration/docs/STAGE06_CLOSEOUT_EXECUTION_CHECKLIST.md`
- `05_STAGE06_STAGE07_PLAN.md`

## 如果你准备直接进入下一阶段

建议继续阅读：

- `05_STAGE06_STAGE07_PLAN.md`
- `../stage07_real_queue_model/START_HERE.md`
