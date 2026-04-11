# Day31 测试结果分析

## 1. 当前代码的默认收口

针对前面出现的 QEMU timeout 问题，day31 当前代码已经做了四项默认收口：

- `QEMU_TIMEOUT_SEC=360`
- `DAY31_BENCH_ITER=200`
- `DAY31_BENCH_WARMUP=20`
- `DAY31_RUN_BENCH_ALL=0`

同时驱动默认关闭 hot path 的 verbose 日志，避免 `irq handler / run_dma start / run_dma ok` 在 bench 过程中大量刷屏，进一步拖慢 QEMU。

## 2. 这轮 records 说明了什么

包内 `records/day31-local-001` 说明默认主链路已经完整跑通：

- guest 串口里出现了 `===DAY31:COMPLETE===`
- `run-summary.md` 中 `guest flow complete: yes`
- `qemu timeout hit: no`
- `bench dma partial: no`

这表明前面那种“bench-dma 被 timeout 截断”的问题，在当前默认配置下已经被消化掉了。

## 3. 通过项逐条分析

### 3.1 EDU 与驱动基础链路

`run-summary.md` 显示：

- `edu device visible: yes`
- `probe logged: yes`
- `dma_alloc_coherent logged: yes`

这三项说明：

- guest 中 EDU 设备已被 `lspci` 正确枚举
- day31 驱动已经 probe 成功
- coherent DMA buffer 已成功分配

这是 day31 后续所有 bench 成立的前提。

### 3.2 mmap-verify

`mmap-verify.txt` 显示：

- `verify_ok=1`
- `mismatch_index=-1`
- `run_ok=1`
- `run_error=0`
- `irq_delta=2`
- `mmap_ok=1`

这说明：

- 用户态 `mmap()` 成功
- 用户态写入的 src 与 DMA 回写后的 dst 完全一致
- 单次 DMA round-trip 完整执行
- 两次 IRQ 都按预期出现

这是 day31 bench 主链路“正确性”最核心的证据。

### 3.3 bench-ioctl

`bench-ioctl.txt` 中：

- `success_ops=200`
- `failed_ops=0`
- `avg_us=15.942`
- `p99_us=23.616`

这说明 day31 的控制路径已经能稳定被量化，并且绝大部分操作落在十几微秒量级。

### 3.4 bench-mmap

`bench-mmap.txt` 中：

- `success_ops=200`
- `failed_ops=0`
- `avg_us=0.557`
- `throughput_mbps=886.949`

这说明：

- 用户态直接访问 coherent DMA buffer 的路径已可用
- 这条路径的成本明显低于 ioctl 与 DMA 路径
- day31 已经把 day30 的“mmap 能用”推进成了“mmap 能量化”

### 3.5 bench-dma

`bench-dma.txt` 中：

- `success_ops=200`
- `failed_ops=0`
- `avg_us=200222.804`
- `p50_us=199995.680`
- `p95_us=200369.536`
- `p99_us=208327.968`

这说明：

- DMA 端到端路径已经形成有效统计
- 当前默认 200 次迭代下，分位数很集中，波动较小
- QEMU EDU 的 DMA 路径虽然慢，但这正是 day31 要展示的“不同路径量级差异”

## 4. bench-all 为什么没有真实矩阵数据

`bench-all.txt` 当前只是 skipped 提示，这符合当前代码设计。

原因不是失败，而是：

- 默认主链路已经能回答 day31 这一天最核心的问题
- `bench-all` 只是为了以后扩展 64/256/1024/2048 的完整矩阵
- 它被显式降级为“默认不自动执行”，以避免再次把自动化拖到 timeout 边缘

## 5. 最终判断

当前包内这轮 records 可以支撑下面的结论：

**day31 默认 bench 主链路已经验收通过。**

更具体地说，这一轮已经证明：

- day31 的驱动、`mmap`、DMA、IRQ、guest 自动化都正常
- `ioctl / mmap / dma` 三条路径都拿到了可解释的 bench 结果
- 当前默认配置已不再受 QEMU timeout 问题困扰
- `bench-all` 可以继续作为下一阶段扩展矩阵，而不是阻塞当前验收
