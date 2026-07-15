# 06. 存储性能、排队与 NUMA

存储性能不是一个单一 MB/s 数字。随机/顺序、读/写、block size、iodepth、direct/buffered、文件系统、CPU、NUMA 与设备并发度都会改变结果。

## 6.1 fio workload 必须可描述

| 参数 | 为什么重要 |
| --- | --- |
| rw / readwrite mix | 顺序和随机、读写比例的设备行为不同 |
| bs | 影响 IOPS、合并、cache line 与协议开销 |
| iodepth / numjobs | 决定并发和排队压力 |
| ioengine / direct | 决定 page cache、提交与完成模型 |
| size / runtime / ramp time | 避免短测和缓存预热误导 |
| filename/device | raw block 与 filesystem 路径含义不同 |

报告必须同时给出 IOPS、带宽、延迟分位数、CPU、错误和队列压力。只有带宽时，无法判断是大块顺序吞吐还是小块随机 IOPS。

## 6.2 Little 定律的直觉

在稳定状态下，in-flight I/O 约等于吞吐乘以平均延迟。若 iodepth 很低，高速设备可能吃不满；若 iodepth 很高，吞吐未提升而 p99 上升，说明主要增加的是排队时间。

因此调队列深度不是越大越好。应绘制 iodepth 与吞吐、p50/p99、CPU、错误的曲线，并选择符合 SLO 的拐点。

## 6.3 NUMA 与 queue affinity

真实设备上，PCIe device 的 NUMA node、提交 CPU、completion CPU、DMA buffer 和 filesystem worker 的位置都会影响延迟和带宽。应记录设备 BDF/NUMA、CPU mask、IRQ、hctx mapping、内存 node，并对同 node/跨 node 作对照。

ramdisk 的结果主要受 CPU/内存带宽和 page cache 影响，不能外推为 PCIe NVMe 或网络块设备结论。

## 6.4 对照实验原则

一次只变一个变量；保留 baseline；先 warm-up；多轮独立运行；保存原始输出和版本快照。使用教学 ramdisk 的 fio 只能验证工作负载、文件系统和观测流程，不应作为真实存储介质优化数字。

下一篇：[07 tracepoint 与排障](07_OBSERVABILITY_TRACEPOINTS_AND_DEBUGGING.md)。
