# day31 验收建议

## 1. Day31 的验收重点

Day31 的重点不是“新增一个复杂功能”，而是把已知功能转成能比较、能解释、能复盘的数据。

因此这里的验收分两层：

### 第一层：功能基线仍然成立

- EDU 设备枚举正常
- `probe` 成功
- `dma_alloc_coherent()` 成功
- `mmap-verify` 成功
- guest 自动化完整结束
- 无 panic / oops / DMA mapping error

### 第二层：bench 数据已经可用

- `bench-ioctl` 有输出
- `bench-mmap` 有输出
- `bench-dma` 有输出
- 至少包含 `p50 / p95 / p99`
- 至少包含 `throughput_mbps`
- 至少包含 `cpu_user_pct / cpu_sys_pct`
- 有 raw records 可追溯

## 2. 基于当前 records/day31-local-001 的验收结论

基于包内 `records/day31-local-001`，day31 默认自动化主链路已经满足验收条件，可判定为**通过**。

本轮直接证据如下：

- `run-summary.md`
  - `edu device visible: yes`
  - `probe logged: yes`
  - `dma_alloc_coherent logged: yes`
  - `mmap verify ok: yes`
  - `bench ioctl present: yes`
  - `bench mmap present: yes`
  - `bench dma present: yes`
  - `bench dma partial: no`
  - `guest flow complete: yes`
  - `qemu timeout hit: no`
  - `oops/dma-error/hung/panic found: no`
- `mmap-verify.txt`
  - `verify_ok=1`
  - `run_ok=1`
  - `irq_delta=2`
  - `mmap_ok=1`
- `run-result.txt`
  - `total_run_calls=221`
  - `total_run_ok=221`
  - `total_run_fail=0`
  - `run_ok=1`
  - `run_error=0`
- `bench-ioctl.txt`、`bench-mmap.txt`、`bench-dma.txt`
  - 均形成了非空、非零迭代、可解释的统计结果。

## 3. 本轮 bench 结果解读

### 3.1 ioctl 控制路径

`bench-ioctl.txt` 结果：

- `iterations=200`
- `success_ops=200`
- `avg_us=15.942`
- `p50_us=14.384`
- `p95_us=16.480`
- `p99_us=23.616`

解释：

- 这一组主要衡量“用户态发起一次轻量 ioctl，再返回”的控制开销。
- 平均耗时在十几微秒级，说明 day31 的控制路径本身并不重。
- `max_us=238.880` 高于主体分布，属于单次抖动，不影响整组稳定性判断；因为 `p95/p99` 仍贴近 `avg`。

### 3.2 mmap 用户态路径

`bench-mmap.txt` 结果：

- `iterations=200`
- `success_ops=200`
- `avg_us=0.557`
- `p50_us=0.368`
- `p95_us=0.560`
- `p99_us=0.752`
- `throughput_mbps=886.949`

解释：

- 这组路径不走设备 DMA，只测用户态对映射区的写、拷贝、比对。
- 它明显快于 ioctl 和 DMA 路径，说明 zero-copy 的“用户可见 buffer 访问”这件事本身成本很低。
- 这也是 day30 → day31 的核心收获：用户态直访 coherent DMA buffer 是可行且很轻的。

### 3.3 DMA 端到端路径

`bench-dma.txt` 结果：

- `iterations=200`
- `success_ops=200`
- `avg_us=200222.804`
- `p50_us=199995.680`
- `p95_us=200369.536`
- `p99_us=208327.968`
- `throughput_mbps=0.009`

解释：

- 这组路径包含：用户态准备 src/dst、两段 EDU DMA、两次 IRQ、返回用户态比较。
- 它比 mmap 路径慢很多，这是符合 QEMU EDU 教学设备特征的；day31 的价值不在于追求绝对性能，而在于把“控制路径 / 用户态路径 / 设备路径”的量级差异定量展示出来。
- `p50/p95/p99` 非常接近，说明当前默认 200 次迭代下，DMA 路径虽然慢，但抖动并不大，分布稳定。

## 4. 关于 bench-all 的口径

`bench-all.txt` 当前内容是：

```text
bench-all skipped by default; set day31_run_bench_all=1 to enable
```

这不是失败，也不是缺项。

这是 day31 当前代码的**默认设计**：

- 默认自动化只跑 `mmap-verify / bench-ioctl / bench-mmap / bench-dma`
- `bench-all` 作为扩展矩阵，只有显式设置 `DAY31_RUN_BENCH_ALL=1` 才开启

因此，默认验收不要求 `bench-all` 必须有真实矩阵结果。

## 5. 最终结论

基于当前包内 records，可以给出如下结论：

**day31 默认 bench 主链路验收通过。**

它已经完成了 day31 这一天最重要的目标：

- 把 day30 的 `mmap` + coherent DMA 主链路量化成了可读的 bench 数据
- 给出了 `ioctl / mmap / dma` 三条路径的分位数、吞吐与 CPU 占用
- 形成了完整 records，可供后续继续扩展 `bench-all` 或更大负载矩阵
