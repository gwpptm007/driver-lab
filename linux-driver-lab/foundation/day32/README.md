# day32：perf：热点采集与一个优化点

## 1. 今日定位

- 周期：W5
- 后端设备：QEMU EDU（DMA-capable）
- 当日目标：围绕 day31 已通过主链路，完成一次热点采集思路固化、一次最小优化，以及一份前后对比报告。

Day32 不再追求“再发明一个功能”，而是回答：

- 热点到底在哪
- 优化点为什么值得做
- 改动前后有没有可复读的证据

## 2. 本日最小闭环

Day32 选择一个**最小且能当天闭环**的优化点：

- baseline：每轮都重新 `GET_INFO + mmap + munmap`
- optimized：bench 前准备一次 `GET_INFO + mmap`，循环内只跑核心 memcpy/compare

这个点的好处是：

- 非常适合配合 `perf` 看 syscall / VMA 管理热点
- 改动小，证据链清晰
- 能和 day30/day31 的 mmap 主链路自然衔接

## 3. 当前 records 的结论

基于包内 `records/day32-local-001`，day32 **默认主链路验收通过**。

关键证据：

- `mmap-verify` 通过：`verify_ok=1`、`run_ok=1`、`irq_delta=2`
- `bench-mmap-baseline` 与 `bench-mmap`（optimized）均有完整结果
- `compare-mmap` 已形成前后对比
- `bench-ioctl` 与 `bench-dma-lite` 均有有效留证
- guest 流程完整结束，且无 panic / oops / DMA mapping error

核心数值：

- baseline `avg_us=283.590`，optimized `avg_us=0.892`
- baseline `p99_us=641.344`，optimized `p99_us=0.912`
- baseline `throughput_mbps=6.585`，optimized `throughput_mbps=1575.094`
- `avg_latency_gain_pct=99.65`
- `p99_latency_gain_pct=99.85`
- `throughput_gain_pct=24826.37`

## 4. 目录重点

- `driver/`：独立 day32 驱动，继续提供 DMA + mmap 基线
- `tools/day32_edu_perf_tool.c`：包含 baseline/optimized 两条 mmap 路径
- `guest/init.day32`：默认 full 流程，输出对比结果与轻量 DMA 对照
- `scripts/11_collect_perf_baseline.sh`：宿主 perf 包裹 baseline workload
- `scripts/12_collect_perf_optimized.sh`：宿主 perf 包裹 optimized workload
- `scripts/13_compare_perf_reports.sh`：从当前 records 提取前后对比摘要

## 5. 默认执行什么

默认 `make run` 会做：

1. `mmap-verify`
2. `bench-mmap-baseline`
3. `bench-mmap`（optimized）
4. `compare-mmap`
5. `bench-ioctl`
6. `bench-dma-lite`

其中 `bench-dma-lite` 只是保留一条轻量设备对照，不是 day32 的主热点对象。

## 6. 最重要的一句话

Day32 的优化点不是“把所有路径都优化一遍”，而是先把 **mmap benchmark 中重复准备工作搬出热循环**，并用 records 与可选的宿主 perf 共同证明它值不值得保留。
