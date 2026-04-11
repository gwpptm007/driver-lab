# Day35 指标索引说明

## 1. 关键指标来源

- Day29：`verify_ok / irq_delta`
- Day30：`mmap_ok / verify_ok / run_ok`
- Day31：`avg_us / p99_us / throughput_mbps`
- Day32：`avg_latency_gain_pct / p99_latency_gain_pct / throughput_gain_pct`
- Day34：`completed_loops / failed_loops / fault errno`

## 2. 证据索引原则

Day35 不复制原始日志全文。
Day35 只做两件事：

- 记录“证据文件在哪里”
- 记录“这个文件证明了什么”

## 3. 指标汇总 CSV 的用途

`day35_metrics_summary.csv` 不是为了好看，而是为了：

- 后续阶段对比
- 面试时快速引用具体数字
- 后续补实验时直接增量扩展
