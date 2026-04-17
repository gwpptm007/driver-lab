# STAGE_OVERVIEW

## stage06 核心目标

> **如何把前面已经做通的教学型 netdev 实验，迁到 ARM64，并把整个过程沉淀成跨平台方法。**

关键词：migration、compatibility、parameterization、closure

**一句话边界**：stage06 不是"新功能驱动阶段"，而是"平台迁移与跨平台收口阶段"。

### 本阶段要做
- 平台迁移（host / qemu-x86_64 / qemu-arm64）
- 兼容层抽象
- 构建/运行/回归收口
- 为 stage04 及后续驱动准备可复用基座

### 本阶段先不做
新 ring 语义、NAPI 理论创新、RX replenishment 新机制、多队列、RSS/offload/XDP

---

## 迁移策略：northbound 保持 / southbound 迁移

### 保持不变（northbound）
- `net_device` 生命周期理解
- `ndo_start_xmit` / NAPI / stats 观察口径
- stage04 的测试方法
- records / output / smoke 报告组织方式

### 需要迁移（southbound）
- 构建路径、工具链
- QEMU 机型与参数
- kernel image / rootfs image 路径
- 运行环境假设
- 某些内核 API 差异

---

## 三条平台路径

### 1. host
- 用途：快速检查脚本/环境/构建入口，作为回归对照基线

### 2. qemu-x86_64
- 用途：在不引入交叉编译的前提下，先把 QEMU 链路走通，作为 ARM64 之前的中间桥梁

### 3. qemu-arm64
- 用途：真正完成 stage06 的目标，验证工具链、镜像、运行方式与前面实验是否可迁移

### 为什么不只留 ARM64
因为 stage06 的重点不是单一平台成功，而是建立可比较的跨平台方法。

---

## ARM64 迁移三步法

```
第一步：工具链就绪
  ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-  ✓

第二步：符号表就绪（最常出问题的地方）
  vmlinux.symvers 必须包含 netdev 核心符号
  → 缺少符号 = kernel build 时 CONFIG_NET=n

第三步：rootfs 就绪
  busybox ARM64 + af_packet.ko + 模块 + 工具  ✓
  → init 脚本用 #!/bin/sh
  → mkdir -p /proc /sys /dev
```

---

## 与 virtio-net 的结构映射

stage06 的价值不在于"又写了一份新驱动"，而在于把平台差异从主逻辑中剥离，用脚本、env、兼容层把迁移方法沉淀下来。

---

## 与 stage07 的边界关系

- **stage06** 解决平台迁移与收口
- **stage07** 解决更真实的 queue / completion / notify 模型

这两者是连续关系，但不能混成一个阶段。
