# driver-lab

一个以 **Linux 驱动学习 + 实验环境搭建 + 阶段交付收口** 为主线的项目仓库。

这套仓库不是只放单个 demo，而是把：

- 内核与 BusyBox 实验环境准备
- 每日驱动代码与脚本
- 回归记录与证据归档
- 阶段总结与交付文档

放在同一个项目里持续演进。

---

## 1. 仓库分层

```text
driver-lab/
├── kernel-src/          内核 / BusyBox 环境准备说明与目录骨架
└── linux-driver-lab/    foundation / netdev / track 专题的代码、脚本、文档、records、阶段报告
```

### `kernel-src/`
负责说明实验环境依赖，包括：

- Linux 内核源码与构建目录
- BusyBox 源码与最小 rootfs 产物
- x86 / arm64 的基础目录约定

### `linux-driver-lab/`
负责真正的学习主线，当前已经覆盖：

- **W1：字符设备基础闭环**（day01 ~ day07）
- **W2：platform / DT / IRQ / regmap / ftrace**（day08 ~ day14）
- **W3：baseline / profile / perf / 回归收口**（day15 ~ day21）
- **W4：PCIe 基本功作品线**（day22 ~ day28）
- **W5：DMA / mmap / bench / perf / ftrace / stability**（day29 ~ day35）
- **netdev 主线**（stage00 ~ stage14）：net_device / skb / NAPI / ring / XDP
- **专题 track**：real-driver / virtual-net / DPDK / AF_XDP / eBPF observability
- **下一主线规划**：DPDK Advanced → RDMA → SmartNIC/DPU
- **P2 保留支线**：block I/O / storage I/O（bio / request / blk-mq / fio / observability）

也就是说，仓库当前状态已经不再是“入门样例集合”，而是一条比较完整的驱动实验型学习路线。

---

## 2. 当前推荐阅读顺序

第一次看这个仓库，建议按下面顺序进入：

1. `kernel-src/README.md`
2. `linux-driver-lab/docs/05_START_HERE.md`
3. `linux-driver-lab/docs/01_PROGRAMS.md`
4. `linux-driver-lab/docs/03_PROGRESS.md`
5. `linux-driver-lab/README.md`
6. 按阶段阅读：
   - W3 总结入口：`linux-driver-lab/foundation/day21/FINAL_SUBMISSION.md`
   - W4 总结入口：`linux-driver-lab/foundation/day28/README.md`
   - W5 总结入口：`linux-driver-lab/foundation/day35/README.md`
   - netdev 总入口：`linux-driver-lab/netdev/README.md`
   - track 总览：`linux-driver-lab/docs/01_PROGRAMS.md`

---

## 3. 当前仓库最适合怎么用

### 用法 A：按学习路径逐天推进
从 `linux-driver-lab/foundation/day01` 开始按 day 阅读与复现。

### 用法 B：按阶段复盘
如果你不是第一次看，建议直接看：

- W1~W5 总评审：`linux-driver-lab/docs/07_FOUNDATION_REVIEWS.md`
- W3：`linux-driver-lab/foundation/day21/FINAL_SUBMISSION.md`
- W4：`linux-driver-lab/foundation/day28/README.md`
- W5：`linux-driver-lab/foundation/day35/README.md`
- netdev：`linux-driver-lab/netdev/README.md`
- track：`linux-driver-lab/docs/01_PROGRAMS.md`

### 用法 C：做项目评审
优先看：

- `linux-driver-lab/docs/05_START_HERE.md`
- `linux-driver-lab/docs/01_PROGRAMS.md`
- `linux-driver-lab/docs/03_PROGRESS.md`

这 3 个文件是当前项目总览、阶段定位和完成度矩阵的主要入口。

---

## 4. 当前项目状态一句话结论

> 当前仓库已经完成从字符设备基础，到 platform/DT/IRQ，再到 PCIe/DMA/性能分析与稳定性验证的主线闭环；
> AF_XDP 四阶段实验（XDP redirect → socket/rings → zero-copy → mini forwarder）全部复测通过（2026-06-07）；
> network data plane 作品集已通过 `network-data-plane-v1` 标签封版；
> 下一阶段主线调整为 DPDK Advanced → RDMA → SmartNIC/DPU，面向高性能网络、RDMA 和数据中心网络加速方向；
> block I/O / storage I/O 规划先作为 P2 支线保留。

---

## 5. 说明

仓库中不提交 Linux 和 BusyBox 的完整源码内容。

使用时请先根据 `kernel-src/README.md` 准备本地内核与 BusyBox 目录，再按 `linux-driver-lab/` 中各 day 的说明执行。
