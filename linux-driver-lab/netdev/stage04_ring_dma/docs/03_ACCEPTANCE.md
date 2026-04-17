# ACCEPTANCE

## 验收标准

### 基本通过

- [ ] sender / receiver 能在 nds4 上看到闭环
- [ ] `debugfs` 能看到 TX/RX、DMA、refill 计数
- [ ] poll 能 drain ring

### 进阶通过

- [ ] ring dump 能解释 ownership/state 变化
- [ ] ring_size / napi_weight 变化会影响行为
- [ ] 可以用 burst 看到预算耗尽或 ring 紧张的现象

---

## 通过口径

### 验收 1：TX / RX 闭环

- [ ] ARM64 交叉编译成功（`netdev_stage04.ko` 为 aarch64 ELF）
- [ ] 模块在 ARM64 上成功加载并注册 netdev（nds4）
- [ ] NAPI poll + ring DMA + RX replenishment 在 ARM64 上正常工作
- [ ] smoke test 端到端通过

### 验收 2：环境验收

- [ ] `resolve-host` 能在无个人路径前提下解析完成
- [ ] `resolve-x86` 能在无个人路径前提下解析完成
- [ ] `resolve-arm64` 能在无个人路径前提下解析完成

### 验收 3：构建验收

- [ ] host 构建路径可独立执行
- [ ] arm64 构建路径可独立执行
- [ ] 模块产物进入固定输出目录
- [ ] 构建失败时能快速定位是环境问题还是源码问题

### 验收 4：运行时 smoke 验收

- [ ] 模块可加载
- [ ] netdev 注册成功
- [ ] 至少一轮 TX→RX 闭环成功
- [ ] NAPI poll 被触发
- [ ] RX replenish 正常发生
- [ ] dmesg 中无关键报错

---

## 通过标准

> **不依赖个人路径、不依赖人工记忆步骤、不把主驱动逻辑和平台脚手架混在一起。**

只要以上 4 项全部满足，即可判定 stage04 完成。
