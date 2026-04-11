# day32 详细计划

## 1. 今日主题
perf：热点采集与一个优化点

## 2. 核心目标

围绕 day31 已通过的 mmap 主链路，完成：

- baseline workload
- optimized workload
- perf 采集入口
- 前后对比摘要

## 3. 选择的优化点

Day32 不直接去优化 QEMU EDU 的 DMA 仿真，而是先优化**用户态 mmap bench 自身的热循环**。

### baseline
每轮循环都做：

1. `GET_INFO`
2. `mmap`
3. 数据准备与 compare
4. `munmap`

### optimized
在循环外一次性完成：

1. `GET_INFO`
2. `mmap`

循环内只做：

1. 数据准备
2. `memcpy/memcmp`

## 4. 为什么先做这个点

- 容易被 perf 捕捉
- 不依赖更深的硬件仿真优化
- 修改小，结论容易讲清楚

## 5. 当天最小闭环

- `make run` 产出 baseline/optimized/compare 结果
- `make perf-baseline` / `make perf-optimized` 产出宿主 perf 原始输出
- `make compare-perf` 生成摘要
