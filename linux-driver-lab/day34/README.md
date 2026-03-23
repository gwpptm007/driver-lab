# Day34 - 稳定性：并发压测 + 模块循环 + 错误注入

Day34 以 day32/day33 的 coherent DMA + mmap + ioctl 基线为基础，目标不再是功能新增，而是回答三个稳定性问题：

1. 多进程并发访问同一设备时是否会误报失败。
2. `insmod/rmmod` 循环时，PCI/MSI/IRQ/DMA 资源释放是否完全配对。
3. 非法输入是否被明确拒绝，而不是把设备留在半失效状态。

## 当前这轮测试结论

基于包内 `records/day34-local-001`，**当前 Day34 默认主链路验收通过**。

已通过：
- `mmap-verify` 通过，说明主数据路径可用。
- 并发压测通过：3 个 `stress-mmap` worker 与 1 个 `stress-ioctl` worker 全部 `rc=0`，`worker_fail=0`。
- 模块循环通过：`requested_loops=1000`、`completed_loops=1000`、`failed_loops=0`。
- 错误注入通过：
  - `fault-invalid-len` 返回 `EINVAL`；
  - `fault-mmap-offset` 返回 `EINVAL`。
- guest 正常结束，无 `BUG/Oops/panic`，也没有宿主 `timeout` 收尾。

## 这轮最容易误读的点

### `run-result.txt` 里的计数为什么是 0？

这是 **预期现象，不代表整轮测试失败**。

Day34 的 guest 流程顺序是：
1. `mmap-verify`
2. 并发压测
3. 模块循环
4. `fault-invalid-len`
5. `fault-mmap-offset`
6. `result`

而 `result` 读取的是驱动里“最近一次操作”的状态快照。当前最后一步有效操作是
`fault-mmap-offset`，它会把最近一次 mmap 结果更新成：
- `mmap_ok=0`
- `mmap_error=-22`
- `mmap_len=4096`
- `mmap_pgoff=1`

所以 `run-result.txt` 更像是“最后一次 fault 注入后的状态快照”，不是 Day34 整轮稳定性回归的汇总。真正的验收应以：
- `mmap-verify.txt`
- `concurrent-stress.txt`
- `module-loop.txt`
- `fault-invalid-len.txt`
- `fault-mmap-offset.txt`
- `run-summary.md`

这些 records 组合起来判断。

## 这版代码重点收口点

1. `remove()` 采用 `free_irq() -> pci_free_irq_vectors() -> dma_free_coherent()` 的顺序，保证 1000 次模块循环可稳定完成。
2. `stress-mmap` 在用户态对“填充 -> RUN_DMA -> compare”整段加 `flock()`，避免多 worker 直接踩共享 src/dst 半页，把共享缓冲区竞争误报成驱动不稳定。
3. `build-lspci` 强制静态链接，避免 guest 中出现误导性的 `/bin/lspci: not found`。
