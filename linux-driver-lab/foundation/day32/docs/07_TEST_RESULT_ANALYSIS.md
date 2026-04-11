# Day32 测试结果分析

## 1. 本轮结论

基于 `records/day32-local-001`，Day32 **默认主链路通过**。

这轮 records 已经同时证明了三件事：

1. day30/day31 继承下来的 `coherent DMA + mmap` 主链路是稳定的。
2. Day32 设计的最小优化点（把 `GET_INFO + mmap + munmap` 搬出热循环）确实带来了显著收益。
3. 轻量 DMA 对照路径仍然能跑通，因此优化并没有破坏设备参与链路。

## 2. 先看哪些文件

- `records/day32-local-001/run-summary.md`
- `records/day32-local-001/mmap-verify.txt`
- `records/day32-local-001/bench-mmap-baseline.txt`
- `records/day32-local-001/bench-mmap-optimized.txt`
- `records/day32-local-001/compare-mmap.txt`
- `records/day32-local-001/bench-ioctl.txt`
- `records/day32-local-001/bench-dma-lite.txt`
- `records/day32-local-001/run-result.txt`

## 3. 主链路为什么可以判定为通过

### 3.1 功能链路通过

`mmap-verify.txt` 中已经出现：

- `verify_ok=1`
- `run_ok=1`
- `irq_delta=2`
- `mmap_ok=1`

这说明：

- 用户态能够成功映射 coherent DMA buffer
- 用户态写入的 src 与设备回写的 dst 一致
- EDU round-trip 两段 DMA 都完成并触发了预期 IRQ

### 3.2 运行稳定性通过

`run-summary.md` 中出现：

- `guest flow complete: yes`
- `qemu timeout hit: no`
- `oops/dma-error/hung/panic found: no`

说明这轮不是“局部命令成功”，而是**整条 day32 默认流程完整结束**。

### 3.3 设备链路未被优化破坏

`bench-dma-lite.txt` 中：

- `mode=dma`
- `iterations=32`
- `success_ops=32`
- `failed_ops=0`
- `avg_us=201041.057`

它虽然不快，但这是 QEMU EDU 仿真设备的正常教学特征。关键点是：

- DMA 对照链路存在
- 结果稳定
- 优化点只针对 mmap bench，不影响设备路径正确性

## 4. Day32 的优化点为什么成立

Day32 选择的优化点不是改 DMA 寄存器，也不是改中断逻辑，而是：

- baseline：每轮重复执行 `GET_INFO + mmap + munmap`
- optimized：循环外做一次 `GET_INFO + mmap`，循环内只做核心数据路径

这让 baseline 与 optimized 唯一显著差异落在：

- syscall 次数
- VMA 建立/销毁次数
- 热循环中的准备动作是否重复

因此 day32 的对比结论具备可解释性。

## 5. 本轮量化结果怎么解读

### 5.1 平均延迟

- baseline `avg_us=283.590`
- optimized `avg_us=0.892`
- `avg_latency_gain_pct=99.65`

这说明把 `mmap/munmap` 从热循环移出后，平均单次开销大幅下降。换句话说，baseline 的主要成本不是 memcpy 本身，而是“每轮重复准备”。

### 5.2 尾延迟

- baseline `p99_us=641.344`
- optimized `p99_us=0.912`
- `p99_latency_gain_pct=99.85`

这说明优化不仅降低平均值，也显著压缩了尾部抖动。对 bench 类工作负载来说，这一点和平均值一样重要。

### 5.3 吞吐

- baseline `throughput_mbps=6.585`
- optimized `throughput_mbps=1575.094`
- `throughput_gain_pct=24826.37`

吞吐的大幅提升，本质上是同一件事的另一面：

- baseline 把时间花在了反复建立/销毁映射上
- optimized 把时间留给了真正的数据访问路径

## 6. bench-ioctl 与 bench-dma-lite 在本轮中承担什么角色

### bench-ioctl

`bench-ioctl.txt` 显示：

- `avg_us=16.291`
- `p99_us=46.160`

它提供的是“控制路径”对照。它比 optimized mmap 慢，但远快于 DMA，对 day32 有两个作用：

1. 证明 guest 用户态工具与驱动的 ioctl 控制路径本身没有异常
2. 给 mmap bench 一个非 DMA 的系统调用型参考点

### bench-dma-lite

`bench-dma-lite.txt` 显示：

- `avg_us≈201ms`
- `success_rate=100%`

它不是性能目标，而是“设备参与路径仍然通”的对照项。

## 7. 本轮还有什么没有覆盖

- `host perf baseline/optimized` 结果本轮未生成
- 因此 day32 当前的“热点解释”更多依赖 workload 设计与 timing 对比，而不是宿主 perf report 文本

这不影响本轮默认主链路通过，但如果后续想把 day32 做成更完整的 perf 留证版，建议再补：

- `make perf-baseline`
- `make perf-optimized`
- `make compare-perf`

## 8. 最终判断

本轮 day32 可以判定为：

- **功能通过**：`mmap-verify`、`bench-ioctl`、`bench-mmap-baseline`、`bench-mmap`、`bench-dma-lite` 全部成功
- **优化有效**：`compare-mmap` 显示平均延迟、尾延迟、吞吐都显著改善
- **稳定性通过**：无超时、无 panic、无 DMA mapping error，guest 完整结束

因此，Day32 当前可以作为 **验收通过版** 留档。
