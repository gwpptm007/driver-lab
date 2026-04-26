# 01_GOAL_AND_SCOPE

## 目标

这个 Lab 的目标不是“再学一个驱动名词”，而是形成：

- 第二个真实驱动专题
- 与 `virtio_net` 的系统对照
- 与自己 `netdev/stage00~stage13` 的第二轮映射

## 为什么这一步重要

如果只停在 `virtio_net`，你的真实驱动视角会偏单一。  
增加 `e1000/e1000e` 之后，你就会同时拥有：

- 半虚拟化 NIC 视角
- 传统 PCI NIC 视角

这会让后续：
- 真实驱动 patch
- 系统协同
- 面试表达
都更稳。

## 当前范围

### 当前做什么
- 设备模型 / probe / remove
- TX / RX 主路径
- IRQ / NAPI / queue 组织
- ethtool / stats
- 与 `virtio_net`、`netdev/stage00~stage13` 对照

### 当前不做什么
- 大 patch
- 大规模 benchmark
- 多驱动并行深入
