# Day33 Trace Summary

## 当前包内 records 结论

基于 `records/day33-local-001`：

- `mmap-verify`：通过
- `function_graph`：未成功开启
- `trace-window.txt`：为空
- `guest complete`：未达成
- `panic`：出现

## 根因

tracefs 根目录被假设为 `/sys/kernel/tracing`，当前 guest 环境中该路径不可写，导致 `/init` 在 trace 配置阶段退出。

## 这版代码状态

当前 day33 代码已经补上：

- `/sys/kernel/tracing` 与 `/sys/kernel/debug/tracing` 的动态探测
- `trace_setup_failed=...` 失败标记
- trace 配置失败时优雅退出，而不是 panic

## 下一轮复测关注点

新的 `records/day33-local-001/run-summary.md` 至少应满足：

- `trace config function_graph: yes`
- `trace window present: yes`
- `trace mentions day33_ioctl: yes`
- `trace mentions day33_do_run_dma: yes`
- `guest flow complete: yes`
- `oops/dma-error/hung/panic found: no`
