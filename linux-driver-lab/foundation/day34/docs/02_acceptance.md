# Day34 验收口径

## 默认目标

- `mmap-verify` 成功
- 并发压测结束且 `worker_fail=0`
- `insmod/rmmod` 循环完成 1000 次且 `failed_loops=0`
- 非法长度注入返回预期失败
- 非法 `mmap` offset 注入返回预期失败
- guest 正常关机，无 `BUG/Oops/panic`

## 当前 records 结论

基于 `records/day34-local-001`：**通过**。

### 已通过
- `mmap-verify`：`verify_ok=1`
- 并发压测：`worker_total=4`、`worker_fail=0`
- 模块循环：`requested_loops=1000`、`completed_loops=1000`、`failed_loops=0`
- 错误注入：
  - `fault-invalid-len` 返回 `expected_failure=1`
  - `fault-mmap-offset` 返回 `expected_failure=1`
- guest 流程：`===DAY34:COMPLETE===`
- 宿主侧：`qemu timeout hit: no`
- 内核侧：无 `BUG/Oops/panic`

### 特别说明：`run-result.txt`
- 当前 `run-result.txt` 反映的是**最后一次 fault-mmap-offset 操作后的状态快照**。
- 因为最后一步是非法页偏移 `mmap`，所以它显示：
  - `mmap_ok=0`
  - `mmap_error=-22`
  - `mmap_len=4096`
  - `mmap_pgoff=1`
- 这不是 Day34 整轮失败，而是驱动正确记录了“最后一次错误注入”的结果。
