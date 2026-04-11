# day33 详细计划

## 1. 今日主题

ftrace `function_graph` 关键路径解释。

## 2. 核心目标

围绕一条固定 workload 采集 `function_graph`，并回答三个问题：

1. 这条 workload 真实经过了哪些关键函数？
2. 哪一段调用最耗时？
3. 这些耗时是否与 day31/day32 的 bench 结论互相印证？

## 3. 为什么默认 workload 选 `mmap-verify`

相比大规模 `bench-dma`，`mmap-verify` 有几个明显优势：

- 一次执行就能覆盖 `RUN_DMA` 主链路
- trace 更短，更适合人工阅读
- 仍然能看到 `program_dma / wait_dma_idle / irq_handler` 这些关键函数

## 4. 今日最小闭环

- 输入：day32 已通过的 `mmap + dma` 代码基线
- 过程：配置 tracefs -> 执行 workload -> 导出 trace -> 人工解释
- 输出：`trace-window.txt` + `day33_trace_summary.md`

## 5. 建议当天保留的证据

- guest 中的 tracing 配置原文
- `mmap-verify` 原始输出
- `trace-window.txt`
- `run-result.txt`
- `dmesg-driver.txt`

## 6. 当天结束前自查

- 主链路是否完成
- trace 是否真的命中了关键函数
- `records/` 是否可供别人复盘
