# Day32 指标与报告口径

## 1. timing 与 perf 两层证据

Day32 默认保留两层证据：

- 工具自身 timing：直接体现优化前后延迟与吞吐变化
- 宿主 perf：辅助解释热点是否集中在 syscall / mmap / munmap 路径

## 2. compare-mmap 输出解释

`compare-mmap.txt` 重点看：

- `baseline_avg_us`
- `optimized_avg_us`
- `avg_latency_gain_pct`
- `baseline_p99_us`
- `optimized_p99_us`
- `throughput_gain_pct`

## 3. host perf 输出解释

- `host-perf-baseline.stat.txt`
- `host-perf-optimized.stat.txt`
- `host-perf-baseline.report.txt`
- `host-perf-optimized.report.txt`

这些文件不是默认必须项，但一旦生成，就能用来解释“为什么优化有效”。
