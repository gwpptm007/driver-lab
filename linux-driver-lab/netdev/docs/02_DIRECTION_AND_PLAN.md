# 02. 学习方向与总体计划

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

---

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
不是"又一种收包 API"，而是解决高频包场景下的中断风暴与批处理问题。

### 4. ring / descriptor 到底在做什么
ring 不是目的，而是 `skb` 在驱动与设备之间转移所有权和搬运数据的机制。

### 5. 为什么 RX replenishment 是关键难点
因为 RX buffer 的生命周期不是"一次申请，反复使用"，而是：

- 预填充
- 设备持有
- 完成后交回
- 驱动消费
- 再次补充

---

## 三、这条线明确不做什么

### 1. 不把第一阶段做成"真实商用网卡"
第一阶段是教学型、可解释型、可验证型实现。

### 2. 不在 stage01~stage04 里强绑 ARM64
平台迁移留到后两阶段做。

### 3. 不在早期阶段混入过多 offload / 多队列 / XDP
先把主路径吃透，再扩展。

---

# 阶段总览

### Stage00：bootstrap
目标：搭一个架构中立、平台可参数化、可报告的启动骨架。

### Stage01：netdev skeleton
目标：最小 `net_device` 骨架，理解生命周期与最小注册闭环。

### Stage02：skb path
目标：围绕 `skb` 建立软件 TX/RX 闭环，先理解"处理什么"。

### Stage03：NAPI poll
目标：理解为什么需要 NAPI、poll/budget 语义、中断抑制与观测。

### Stage04：ring + DMA + RX replenishment
目标：把 `skb` 搬运机制正式落到 ring/descriptor/DMA 语义上。

### Stage05：virtio-net + parameterization
目标：做一次真实实现对照，并把工程参数化，为跨平台迁移做准备。

### Stage06：ARM64 migration + cross-platform closure
目标：把前面建立的 netdev 作品线迁移到 ARM64，并完成跨平台回归与总结。

---

## 为什么是这个顺序

### 骨架 → skb
先理解驱动入口与网络包对象。

### skb → NAPI
先理解收发对象，再理解为什么需要批处理和中断抑制。

### NAPI → ring/DMA
先理解上层语义，再理解底层搬运机制。

### ring/DMA → virtio-net
自己做过之后再看成熟实现，收益更高。

### virtio-net → ARM64
先把实现和工程抽象好，再做迁移，更容易定位差异。

---

## 时间估计（建议值）

- Stage00：2~3 天
- Stage01：3~4 天
- Stage02：4~6 天
- Stage03：4~7 天
- Stage04：6~10 天
- Stage05：5~8 天
- Stage06：5~8 天

**总计建议：4~6 周。**

---

## Gate 机制

### Gate A（Stage02 结束）
必须能讲清：

- `net_device` 生命周期
- `skb` 基本结构
- 软件 TX/RX 闭环

### Gate B（Stage03 结束）
必须能验证：

- NAPI poll 确实在工作
- 中断与轮询职责边界清楚
- 有自己的统计项与观测证据

### Gate C（Stage04 结束）
必须能解释：

- descriptor ownership
- RX replenishment
- ring 枯竭与 refill 行为
- DMA map/unmap 生命周期

---

# 阶段任务拆解

## Stage00：bootstrap

### 目标
- 架构中立目录骨架
- 依赖检查
- 变量化 env/scripts
- 启动报告

### 产出
- `Makefile`
- `env/stage00.env`
- `scripts/discover_paths.sh`
- `scripts/check_host_tools.sh`
- `scripts/generate_stage00_report.sh`

---

## Stage01：netdev skeleton

### 任务
- 最小 `net_device` 驱动骨架
- `alloc_netdev_mqs` / `register_netdev`
- `ndo_open/stop/start_xmit`
- 最小 stats / debugfs

### 产出
- 驱动源码
- 最小用户态控制工具
- lifecycle 说明文档

---

## Stage02：skb path

### 任务
- 理解 `skb` 基本字段
- 软件 TX/RX 闭环
- `netif_rx` / `napi_gro_receive` 选择口径说明
- 内部环回 / 注入机制

### 产出
- `skb` 路径实验
- 数据流说明图
- 最小回归脚本

---

## Stage03：NAPI poll

### 任务
- 加入 NAPI
- poll + budget
- 中断抑制 / 恢复语义
- 统计项：irq / poll / budget hit / rx batches

### 产出
- NAPI 实验报告
- 纯中断 vs NAPI 对比
- 观测脚本

---

## Stage04：ring / DMA / RX replenishment

### 任务
- descriptor/ring 设计
- ownership 语义
- streaming DMA map/unmap
- RX buffer 预填充与 refill
- ring 枯竭行为说明

### 产出
- ring 设计文档
- DMA 路径代码
- refill 指标和错误注入

---

## Stage05：virtio-net + parameterization

### 任务
- 精读 `virtio-net` 主路径
- 对照自研 ring 与 vring
- 将 env/scripts 变为平台可配置
- 为 ARM64 迁移列出差异清单

### 产出
- 对照分析文档
- 参数化改造清单
- 路线评审结论

---

## Stage06：ARM64 migration

### 任务
- 迁移到 ARM64 QEMU
- 交叉编译与运行脚本
- 跨平台回归
- 性能/观测差异总结

### 产出
- ARM64 运行记录
- 差异报告
- 最终总报告
