# records 说明

当前目录用于保存 day33 每轮运行后的归档结果。

本包内已经带有一轮现场记录：

- `day33-local-001`

这轮记录的结论是：

- `mmap-verify` 成功
- tracefs 配置失败
- guest 触发 panic
- 因而 Day33 当轮不通过

同时，当前代码目录已经包含修复后的 tracefs 探测与失败兜底逻辑，可直接用于下一轮复测。
