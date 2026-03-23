# Day34 测试结果分析

## 当前现场

基于 `records/day34-local-001`，当前这轮主要结论如下：

- `mmap-verify` 成功：`verify_ok=1`、`run_ok=1`、`irq_delta=2`。
- 并发压测成功：
  - 3 个 `stress-mmap` worker 全部 `rc=0`，且 `success_ops=16/16`；
  - 1 个 `stress-ioctl` worker `success_ops=200/200`；
  - 汇总 `worker_fail=0`。
- 模块循环成功：`requested_loops=1000`、`completed_loops=1000`、`failed_loops=0`。
- 两条错误注入都成功命中驱动拒绝分支：
  - 非法长度返回 `EINVAL`；
  - 非法页偏移 `mmap` 返回 `EINVAL`。
- guest 流程完整结束，无 `BUG/Oops/panic`，也没有宿主 `timeout`。

## 关键证据

### 1. `mmap-verify` 已通过
- `verify_ok=1`
- `run_ok=1`
- `irq_delta=2`
- `mmap_ok=1`

### 2. 并发压测通过
- `worker_1_rc=0`
- `worker_2_rc=0`
- `worker_3_rc=0`
- `worker_ioctl_rc=0`
- `worker_fail=0`

这说明：
- `stress-mmap` 的共享缓冲区协调方案是有效的；
- 并发阶段没有再把共享 `src/dst` 半页的数据竞争误报成驱动不稳定。

### 3. 模块循环通过
- `requested_loops=1000`
- `completed_loops=1000`
- `failed_loops=0`

同时 `dmesg-driver.txt` 可见大量重复的：
- `probe success`
- `remove complete`

这说明当前 `remove()` 路径中的：
- `free_irq()`
- `pci_free_irq_vectors()`
- `dma_free_coherent()`

配对关系已经稳定，不再触发旧版 MSI 释放 BUG。

### 4. 错误注入通过
- `fault-invalid-len.txt`
  - `expected_failure=1`
  - `errno=22`
- `fault-mmap-offset.txt`
  - `expected_failure=1`
  - `errno=22`

这说明非法输入没有把设备留在半失效状态，而是被明确拒绝。

## 关于 `run-result.txt` 的解释

当前 `run-result.txt` 中：
- `total_run_calls=0`
- `mmap_ok=0`
- `mmap_error=-22`
- `mmap_pgoff=1`

这并不表示 Day34 主数据路径失败。原因是：

1. `result` 读取的是驱动里“最近一次操作结果”。
2. guest 在 `result` 前最后执行的是 `fault-mmap-offset 1`。
3. 驱动因此把“最近一次 mmap 操作”记录成：
   - 非法页偏移；
   - `EINVAL`；
   - `mmap_ok=0`。

也就是说，这个文件是**最后一次 fault 注入的快照**，不是 Day34 整轮统计汇总。整轮是否通过，应以 `run-summary.md` 与多个 records 文件组合判断。

## 当前判断

当前 Day34 **通过**。

这版 records 已经同时证明：
1. 主数据路径可用；
2. 并发压测可稳定完成；
3. 1000 次 `insmod/rmmod` 循环可稳定完成；
4. 非法输入会被驱动明确拒绝；
5. guest 流程可完整结束并正常留档。
