# 01. netdev 方向与范围

## 一、为什么在 foundation 后进入 netdev

`foundation/day01~day35` 已经覆盖：

- 字符设备
- platform / DT / IRQ
- PCIe / MMIO / MSI
- DMA / mmap
- perf / function_graph
- 稳定性验证

下一阶段最自然的扩展，不是继续堆同类教学设备，而是进入一个更真实的 Linux 子系统。

`netdev` 是最合适的选择，因为它能够最大化复用已有的：

- 中断知识
- DMA 知识
- 可观测性习惯
- 工程化脚本能力

## 二、这条线要解决的核心问题

### 1. 驱动与协议栈如何交接
也就是：

- `net_device`
- `ndo_open/stop/start_xmit`
- `netif_*` 接口

### 2. 网络包对象是什么
也就是：

- `sk_buff`
- data / head / tail / len
- 线性区与后续扩展理解

### 3. 为什么需要 NAPI
不是“又一种收包 API”，而是解决高频包场景下的中断风暴与批处理问题。

### 4. ring / descriptor 到底在做什么
ring 不是目的，而是 `skb` 在驱动与设备之间转移所有权和搬运数据的机制。

### 5. 为什么 RX replenishment 是关键难点
因为 RX buffer 的生命周期不是“一次申请，反复使用”，而是：

- 预填充
- 设备持有
- 完成后交回
- 驱动消费
- 再次补充

## 三、这条线明确不做什么

### 1. 不把第一阶段做成“真实商用网卡”
第一阶段是教学型、可解释型、可验证型实现。

### 2. 不在 stage01~stage04 里强绑 ARM64
平台迁移留到后两阶段做。

### 3. 不在早期阶段混入过多 offload / 多队列 / XDP
先把主路径吃透，再扩展。
