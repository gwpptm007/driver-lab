# Day33 测试结果分析

## 一、结论

基于当前包内 `records/day33-local-001`，**Day33 这轮还不能判定通过**。

更准确地说：

- 业务路径已经通过
- trace 采集目标尚未达成
- 失败点发生在 tracefs 配置阶段

所以这轮应被归类为：**主链路通过、trace 环境失败**。

## 二、已经通过的项

从 `run-summary.md`、`mmap-verify.txt` 和 `serial.log` 可以确认：

1. EDU 设备枚举通过
2. 驱动 probe 成功
3. `dma_alloc_coherent()` 成功
4. `mmap-verify` 成功
5. 一轮 DMA run 成功

直接证据如下：

### 1) `run-summary.md`

- `edu device visible: yes`
- `probe logged: yes`
- `dma_alloc_coherent logged: yes`
- `mmap verify ok: yes`

### 2) `mmap-verify.txt`

- `verify_ok=1`
- `run_ok=1`
- `run_error=0`
- `irq_delta=2`
- `mmap_ok=1`

这说明 day33 被 trace 的业务路径本身没有问题。

## 三、未通过的项

当前 records 中，下面这些 Day33 核心验收项没有达成：

1. `function_graph` 没有成功开启
2. `trace-window.txt` 为空
3. 没有抓到关键函数
4. guest 没有走到完整结束标记
5. 串口日志里出现了 panic

直接证据：

### 1) `run-summary.md`

- `trace config function_graph: no`
- `trace window present: no`
- `trace mentions day33_ioctl: no`
- `trace mentions day33_do_run_dma: no`
- `trace mentions day33_wait_dma_idle: no`
- `trace mentions day33_irq_handler: no`
- `guest flow complete: no`
- `oops/dma-error/hung/panic found: yes`

### 2) `trace-config.txt`

```text
tracefs=/sys/kernel/tracing
/init: line 82: can't create /sys/kernel/tracing/tracing_on: nonexistent directory
```

### 3) `qemu.stderr.log`

```text
qemu-system-aarch64: terminating on signal 15 from pid ... (timeout)
```

注意这里的 timeout 不是根因，它只是宿主在 guest 已经早死后，等待超时再回收 QEMU。

## 四、根因链条

这轮问题的根因链条很清楚：

1. guest `/init` 假设 tracing 根目录一定是 `/sys/kernel/tracing`
2. 当前环境里该路径不可用或不可写
3. 写 `tracing_on` 失败
4. `/init` 直接退出
5. 内核触发 `Attempted to kill init!`
6. 宿主侧 timeout 最终回收 QEMU

所以这不是 DMA 问题，也不是 `mmap-verify` 问题，而是 **tracefs 路径兼容问题**。

## 五、这版代码已经补了什么

当前包中的代码已经按这个根因做过修复：

1. `guest/init.day33` 不再只认 `/sys/kernel/tracing`
2. 会优先探测：
   - `/sys/kernel/tracing`
   - `/sys/kernel/debug/tracing`
3. 若两者都不可用：
   - 不再 panic
   - 会写出 `trace_setup_failed=...`
   - 会在 `trace-window.txt` 中写 `trace_window_skipped=trace_setup_failed`
   - 然后优雅退出并保留 records

也就是说，这版包里的**代码已经比当前 records 更进一步**，是为下一轮复测准备的。

## 六、对当前包的正确使用方式

这版包不应该被描述成“Day33 已通过”，而应该描述成：

- **包含一轮失败 records 的分析包**
- **同时包含修复后可复测的代码版本**

如果你下一轮复测成功，那么新的通过证据应当至少包括：

- `trace config function_graph: yes`
- `trace window present: yes`
- `trace mentions day33_ioctl: yes`
- `trace mentions day33_do_run_dma: yes`
- `guest flow complete: yes`
- `oops/dma-error/hung/panic found: no`

## 七、一句话总结

**Day33 当前 records 证明：DMA 主链路已经通，但 tracefs 入口兼容没处理好，因此本轮不通过；当前代码已补上兼容与失败兜底，适合直接进入下一轮复测。**
