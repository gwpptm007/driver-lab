# 01_STAGE_GOAL_AND_BOUNDARY

## 本阶段要解决什么

本阶段解决的是：

> **如何把前面已经做通的教学型 netdev 实验，迁到 ARM64，并把整个过程沉淀成跨平台方法。**

这里的关键词是：
- migration
- compatibility
- parameterization
- closure

## 本阶段不解决什么

- 不解决新的 ring 语义设计
- 不解决新的 NAPI 理论问题
- 不解决新的 RX replenishment 机制
- 不试图一次性对齐完整 virtio-net 功能

这些内容在 stage01~stage05 已经有明确阶段归属。

## 为什么以 stage04 为主要迁移对象

因为 stage04 是最完整的“教学型设备驱动心智模型”：
- 有 ring
- 有 ownership
- 有 DMA map/unmap
- 有 RX replenishment
- 有 NAPI poll

把 stage04 迁到 ARM64，最能锻炼：
- 工具链
- QEMU
- kernel build
- 运行脚本
- debug/trace/records 的复用能力
