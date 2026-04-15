# 01_STAGE_GOAL_AND_BOUNDARY

## 阶段目标

`stage07_real_queue_model` 的目标是：

> 在保持教学可解释性的前提下，把当前自研 netdev 的 ring / DMA / NAPI 模型推进到更接近真实队列驱动的组织方式。

## 为什么现在开这个阶段

因为当前 netdev 主线已经有清晰层次：

- `stage03`：讲清 NAPI
- `stage04`：讲清 ring / DMA / refill
- `stage05`：把 `virtio-net` 对照和平台参数化准备好
- `stage06`：把跨平台迁移方法沉淀下来

下一步最值钱的，不是再加更多旁支特性，而是把驱动模型本身做深。

## 阶段边界

### 本阶段要做
- 单队列 queue model
- index 驱动的 queue lifecycle
- notify / irq / napi / completion 边界
- 与 `virtio-net` 的结构映射
- stats / trace / dump 体系

### 本阶段先不做
- 多队列
- RSS / RPS / XPS
- GRO/GSO/TSO/UFO
- XDP
- 真正物理硬件适配
- 极限性能调优

## 一句话边界

> stage07 是“真实驱动建模”的第一跳，不是“功能大杂烩”的起点。
