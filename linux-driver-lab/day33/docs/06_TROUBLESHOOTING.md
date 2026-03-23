# Day33 排障说明

## 1. `current_tracer: Invalid argument`

通常表示内核没有打开 `FUNCTION_GRAPH_TRACER` 或 `TRACING` 相关配置。请先检查：

- `CONFIG_TRACING=y`
- `CONFIG_FTRACE=y`
- `CONFIG_FUNCTION_TRACER=y`
- `CONFIG_FUNCTION_GRAPH_TRACER=y`
- `CONFIG_DEBUG_FS=y`

## 2. `trace-window.txt` 为空

优先看：

- `trace-config.txt` 是否真的切到了 `function_graph`
- `set_graph_function` 是否写入了 day33 关键函数
- 是否在打开 `tracing_on=1` 之后才执行 workload

## 3. 只有 trace，没有 workload 成功证据

这轮不能算通过。必须至少同时满足：

- `mmap-verify.txt` 里 `verify_ok=1`
- `run-result.txt` 有 `run_ok=1`
- `trace-window.txt` 中出现关键函数

## 4. `/sys/kernel/tracing/tracing_on: nonexistent directory`

这通常不是 DMA 主链路失败，而是当前 guest 环境没有把 tracefs 挂在 `/sys/kernel/tracing`。当前包里这轮 records 就是这个问题：

- `mmap-verify` 已经通过
- 但 trace 配置阶段失败
- 旧现场里因此触发了 `Attempted to kill init!`

新版 day33 已改为：

- 先尝试 `mount -t debugfs none /sys/kernel/debug`
- 再尝试 `mount -t tracefs nodev /sys/kernel/tracing`
- 优先使用存在 `available_tracers` 的路径
- 若两者都不可用，记录 `trace_setup_failed=...` 并优雅退出，而不是 panic

## 5. 宿主看到 `QEMU timeout`，是不是说明 trace 太慢？

不一定。当前这轮 records 里，真正根因是 guest 很早就因为 trace 配置失败退出了；宿主侧的 `timeout` 只是晚一点把 QEMU 回收。因此应先看：

- `serial.log`
- `trace-config.txt`
- `qemu.stderr.log`

判断是“guest 早死 + timeout 回收”，还是“guest 持续运行但 trace 太慢”。
