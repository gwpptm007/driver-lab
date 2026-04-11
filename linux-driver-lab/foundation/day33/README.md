# day33：ftrace function_graph 关键路径解释

## 1. 今日定位

- 周期：W5
- 后端设备：QEMU EDU（coherent DMA + mmap 基线延续自 day32）
- 当日目标：对一条固定 workload 采集 `function_graph` 窗口，解释关键调用链与主要耗时点，并把原始 trace 与解释文本一起沉淀到 `records/` / `output/`。

Day33 不再追求“新增一个大功能”，而是把前面已经跑通的路径**看清楚、说清楚**。

## 2. 当前包状态

这版包已经包含：

- 一份 **已跑过的 day33 records**：`records/day33-local-001`
- 一版 **针对 tracefs 路径兼容问题修过的代码**

需要注意：当前 `records/day33-local-001` 对应的是**修复前现场**，因此本轮测试结论是：

- `mmap-verify` 主链路通过
- `function_graph` 采集未成功
- guest 因 tracefs 路径假设错误而 panic
- **当前这轮 records 不能判 Day33 验收通过**

也就是说，这个包的定位是：

- 文档上：把这轮失败结果解释清楚
- 代码上：已经把 tracefs 兼容和失败兜底补进去，供下一轮复测

## 3. 今日主线

选择一条足够小、又能覆盖关键函数的路径：

- `mmap-verify <len> <seed>` 作为默认 trace workload
- 路径近似为：
  - userspace `ioctl(RUN_DMA)`
  - `day33_ioctl`
  - `day33_do_run_dma`
  - `day33_program_dma`
  - `day33_wait_dma_idle`
  - `day33_irq_handler`
  - 返回用户态继续校验 `src/dst`

这条路径比大规模 bench 更适合做 function_graph：

- trace 窗口小
- 层次清楚
- 证据容易解释

## 4. 这轮测试的真实结论

基于 `records/day33-local-001`：

- 已通过：
  - EDU 设备可见
  - 驱动 probe 成功
  - `dma_alloc_coherent()` 成功
  - `mmap-verify` 成功
  - 一轮 DMA run 成功（`run_ok=1`、`irq_delta=2`）
- 未通过：
  - `function_graph` 未成功打开
  - `trace-window.txt` 为空
  - 未出现 `DAY33:COMPLETE`
  - guest 触发 `Attempted to kill init!`

根因不是 DMA 主链路，而是 trace 配置阶段把 tracing 根目录写死成 `/sys/kernel/tracing`，当前 guest 环境中该路径不可写，随后 `/init` 提前退出。

## 5. 当日交付物

- `records/<run-id>/trace-config.txt`
- `records/<run-id>/trace-window.txt`
- `records/<run-id>/mmap-verify.txt`
- `records/<run-id>/run-result.txt`
- `records/<run-id>/run-summary.md`
- `output/day33_ftrace_explain_template.md`
- `output/day33_trace_summary.md`

## 6. 风险提醒

- `function_graph` 打开范围太大，trace 噪音过多
- 只贴原始 trace，不解释“这行代表什么”
- 误把 tracefs 路径配置失败当成驱动失败

## 7. 和前后天的关系

- 输入：day32 已通过的 `mmap + dma` 代码基线
- 输出：为 day34 继续做 tracepoint / 更细粒度观测提供直接证据

## 8. tracefs 兼容说明

Day33 guest 新版会优先探测 `/sys/kernel/tracing`，若不可用则自动回退到 `/sys/kernel/debug/tracing`。若两者都不可用，不再因为 `set -e` 直接 panic，而会在 `trace-config.txt` / `trace-window.txt` 中留下失败标记并正常关机，方便归档排障。
