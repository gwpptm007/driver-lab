# 02. netdev 总体计划与阶段划分

## 阶段总览

### Stage00：bootstrap
目标：搭一个架构中立、平台可参数化、可报告的启动骨架。

### Stage01：netdev skeleton
目标：最小 `net_device` 骨架，理解生命周期与最小注册闭环。

### Stage02：skb path
目标：围绕 `skb` 建立软件 TX/RX 闭环，先理解“处理什么”。

### Stage03：NAPI poll
目标：理解为什么需要 NAPI、poll/budget 语义、中断抑制与观测。

### Stage04：ring + DMA + RX replenishment
目标：把 `skb` 搬运机制正式落到 ring/descriptor/DMA 语义上。

### Stage05：virtio-net + parameterization
目标：做一次真实实现对照，并把工程参数化，为跨平台迁移做准备。

### Stage06：ARM64 migration + cross-platform closure
目标：把前面建立的 netdev 作品线迁移到 ARM64，并完成跨平台回归与总结。

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

## 时间估计（建议值）

- Stage00：2~3 天
- Stage01：3~4 天
- Stage02：4~6 天
- Stage03：4~7 天
- Stage04：6~10 天
- Stage05：5~8 天
- Stage06：5~8 天

总计建议：**4~6 周**。

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
