# day34 并发与错误注入阅读指南

## 1. 并发压测怎么看

关注 `concurrent-stress.txt` 里的这些字段：

- `worker_total`
- `worker_fail`
- `worker_1_rc ... worker_n_rc`
- 每个 worker 的结果片段

如果 `worker_fail=0`，说明默认并发模型没有出现显式失败。

## 2. 模块循环怎么看

关注 `module-loop.txt` 里的：

- `requested_loops`
- `completed_loops`
- `failed_loops`

day34 这里不要求每轮都展开完整日志，但必须把汇总数写清楚。

## 3. 错误注入怎么看

### 非法长度

`fault-invalid-len.txt` 中应看到：

- 驱动拒绝
- 工具标记 `expected_failure=1`

### 非法 offset

`fault-mmap-offset.txt` 中应看到：

- `mmap` 失败
- 工具标记 `expected_failure=1`

## 4. 最终总览怎么看

优先看 `run-summary.md`：

- 是否完整结束
- 是否命中 panic/oops/hung
- 并发、循环、注入三块是否都留到了证据
