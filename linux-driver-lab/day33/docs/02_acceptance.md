# day33 验收清单

## 必须满足

- [x] `mmap-verify` 成功
- [ ] 成功采集 `function_graph` trace
- [ ] trace 中可见关键函数：
  - [ ] `day33_ioctl`
  - [ ] `day33_do_run_dma`
  - [ ] `day33_program_dma`
  - [ ] `day33_wait_dma_idle`
  - [ ] `day33_irq_handler`
- [ ] `trace-window.txt` 与 workload 对应得上
- [x] `records/run-summary.md` 能说明本轮是否完成
- [ ] 无 panic / oops / DMA mapping error

## 基于当前 `records/day33-local-001` 的结论

**当前这轮不通过。**

原因不是 DMA 主链路，而是 trace 配置阶段失败，导致 guest `/init` 退出并触发 panic：

- `mmap-verify.txt` 中已经有 `verify_ok=1`
- `trace-config.txt` 中记录了 `/sys/kernel/tracing/tracing_on: nonexistent directory`
- `run-summary.md` 中：
  - `trace config function_graph: no`
  - `trace window present: no`
  - `guest flow complete: no`
  - `oops/dma-error/hung/panic found: yes`

因此 Day33 当前包的正确定位是：

- 记录并解释这轮失败结果
- 保留修复后的代码，供下一轮复测

## 建议额外补充

- [ ] 给出 3~5 行 trace 片段的人话解释
- [ ] 说明为什么 `wait_dma_idle` 在这条路径上通常最显眼
- [ ] 补一条 DMA 小 workload 的 trace 对照

## trace 配置失败时的判定

若 `mmap-verify` 已通过，但 `trace-config.txt` 中出现 `trace_setup_failed=...`，则说明业务路径正常、trace 环境未就绪。此时 Day33 不能算通过，但应能正常生成 records，而不是 panic。
