# Project Overview

## 项目目标

`project-linux-network-data-plane` 是 `linux-driver-lab` 的网络数据面总收口项目。它不再新增一条独立 demo，而是把已经完成的网络相关主线整理成一个可以对外展示的完整作品。

目标包括：

- 解释 Linux 网络数据面的核心路径和关键机制。
- 把教学型 netdev、真实驱动、虚拟化网络、DPDK、AF_XDP、eBPF observability 串成一条能力链。
- 为每条路径建立可追溯 evidence 索引。
- 输出最终报告、简历材料和面试讲稿。

## 总体路径

```text
foundation
  -> netdev stage00~stage14
  -> real driver track
  -> virtual network track
  -> DPDK track
  -> AF_XDP track
  -> eBPF observability track
  -> project-linux-network-data-plane
```

其中 `foundation` 提供驱动工程基本功，`netdev` 建立内核网络驱动模型，后续 track 分别展开真实驱动、虚拟化、用户态 fastpath、原生 fastpath 和可观测性。

## 核心设计原则

### 1. 按数据面路径组织

本项目不按日期组织，也不按目录堆材料，而是按网络数据面路径组织：

- 内核网络驱动路径。
- 真实驱动源码路径。
- 虚拟化网络路径。
- DPDK 用户态路径。
- AF_XDP 原生 fastpath 路径。
- eBPF 观测路径。

### 2. 每条路径都回答四个问题

```text
目标是什么？
关键机制是什么？
已有实验证明了什么？
当前边界在哪里？
```

### 3. 证据优先

本项目的价值不只是“学过这些概念”，而是每条路径都有：

- README / START_HERE。
- scripts。
- records。
- reports。
- 测试结论或复盘结论。

## 最终交付

| 交付物 | 说明 |
|--------|------|
| `README.md` | 项目总入口 |
| `docs/*.md` | 六条路径和最终架构 |
| `evidence/*.md` | 对应 track 的证据索引 |
| `reports/final_report.md` | 最终报告 |
| `reports/resume_material.md` | 简历材料 |
| `docs/08_INTERVIEW_SHARE_SCRIPT.md` | 面试讲稿 |

## 推荐使用方式

对外评审时，从 `README.md` 进入，先看 `docs/07_FINAL_ARCHITECTURE.md` 和 `reports/final_report.md`。

准备简历时，看 `reports/resume_material.md`。

准备面试时，看 `docs/08_INTERVIEW_SHARE_SCRIPT.md`。

补证据或复盘时，看 `evidence/README.md`。
