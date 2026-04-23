# 01_LAB_OVERVIEW

## 目标

从 `netdev/stage00~stage14` 进入真实驱动源码研究，形成：

- 对 `virtio_net` 的结构化理解
- 对真实数据路径的可讲解能力
- 对自己教学驱动与真实驱动的映射能力
- 为下一步 patch / trace / 虚拟化协同实验准备入口

## 为什么 stage14 之后不继续 stage15

`stage` 更适合承载课程式线性递进。  
当主题变成：

- 真实驱动源码专题
- patch 实验
- 多条线并行推进

继续用 `stage15/stage16/...` 会把“课程推进”和“专题研究”混在一起。

因此这里正式切换到：

- `track-*`
- `lab-*`
- `project-*`

## 本 Lab 的范围

### 这次要做
- `virtio_net` 结构与关键对象
- probe/remove 主骨架
- TX/RX 主路径
- queue / napi / interrupt / notify 的关系
- feature / ethtool / XDP 入口
- `stage00~stage14 ↔ virtio_net` 的映射

### 这次先不做
- 完整 vhost 后端深挖
- QEMU virtio device 实现细节
- tap/bridge/veth 整个 host 网络系统
- AF_XDP
- 大规模性能压测
- 并行阅读多个复杂物理 NIC 驱动
