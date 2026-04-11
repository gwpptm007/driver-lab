# Day32 perf 深度学习指南 - 热点采集与优化验证

## 一、Day32 是什么？

Day32 是 W5 的第四天，承接 Day31 的纯 benchmark，引入 **宿主端 perf/ftrace 性能分析**。

**核心目标**：围绕已通过的 mmap 主链路，完成一次**热点采集思路固化**、一次**最小优化闭环**，以及**前后对比证据**。

Day32 不再追求"再发明一个功能"，而是回答：
- 热点到底在哪
- 优化点为什么值得做
- 改动前后有没有可复读的证据

---

## 二、Day31 → Day32 的核心变化

### 2.1 从"测量"到"优化"

| 维度 | Day31 | Day32 |
|------|-------|-------|
| 目标 | 建立三条路径的性能基准 | 找到热点并做一次最小优化 |
| 主角 | bench 工具 | perf/ftrace 宿主端工具 |
| 输出 | avg/p50/p95/p99/throughput | 热点函数 + 优化前后对比 |
| 改进对象 | 无 | 用户态 mmap 复用 |

### 2.2 Day32 的优化点

```
baseline：每轮都重新 GET_INFO + mmap + munmap
optimized：bench 前准备一次 GET_INFO + mmap，循环内只跑核心 memcpy/compare
```

这个优化点的本质是：**把 VMA 创建/销毁的成本从热路径移除**。

---

## 三、为什么先优化 mmap 而不是 DMA

### 3.1 QEMU EDU DMA 的限制

```
QEMU EDU DMA 的成本来源：
  - 设备仿真（软件模拟 PCIe 事务）
  - IRQ 往返（两次 IRQ per DMA）
  - QEMU 进程调度

这些成本不适合当天闭环优化，更适合后续专题。
```

### 3.2 为什么选 mmap 优化

```
mmap 优化成本低、证据链清晰：
  - 改动小：只在用户态控制 mmap 频率
  - 效果显著：99.65% 延迟降低
  - 适合用 perf 看到 syscall / VMA 管理热点
  - 能和 day30/day31 的 mmap 主链路自然衔接
```

---

## 四、mmap syscall 成本分析

### 4.1 一次 mmap 调用的完整路径

```
用户态:
  mmap() → syscall → 内核
           ↓
        kernel:
  do_sys_mmap() → mmap_region() → call_mmap() → VMA 创建
           ↓
        返回用户态
```

### 4.2 munmap 调用的完整路径

```
用户态:
  munmap() → syscall → 内核
           ↓
        kernel:
  do_mas_munmap() → remove_vma() → VMA 销毁
           ↓
        返回用户态
```

### 4.3 为什么 mmap+munmap 成本高

```
每次 mmap munmap 循环涉及：
  1. 两个 syscall 陷入内核
  2. 两个 VMA（虚拟内存区域）创建/销毁
  3. 页表操作（如果触发 page fault）
  4. 内核锁竞争（VMA 链表/红黑树）

在 baseline 中，这些成本每次迭代都要付一次。
在 optimized 中，这些成本只付一次（在循环前）。
```

---

## 五、perf 工具链详解

### 5.1 perf stat - 统计计数器

```bash
perf stat -d -o output.stat.txt ./bench-mmap
```

`-d` 选项启用：
- Hardware counters（CPU 周期、指令数）
- Data cache counters（L1/L2/Last Level Cache miss）
- Branch mispredict counters

**输出示例**：
```
Performance counter stats for './bench-mmap':

    1,234,567 cycles               # CPU 周期
      567,890 instructions         # 指令数
          1234 cache-misses        # 缓存未命中
          0.45% branch-mispredict # 分支预测错误率
```

### 5.2 perf record - 采样录制

```bash
perf record -F 49 -g -o output.data ./bench-mmap
```

- `-F 49`：每秒 49 次采样（低于 50Hz 避免干扰）
- `-g`：记录调用链（call graph）
- `-o output.data`：输出采样数据文件

### 5.3 perf report - 报告生成

```bash
perf report --stdio -i output.data
```

显示热点函数，按 CPU 占用排序。

---

## 六、perf 与 Day32 的结合

### 6.1 两条被测路径

**baseline 路径**（热路径包含 mmap/munmap）：
```
用户态 bench-mmap:
    for i in iter:
        get_info()
        mmap()        ←── 每次迭代都做
        fill_pattern()
        memcpy(dst, src, len)
        memcmp(src, dst, len)
        munmap()      ←── 每次迭代都做
```

**optimized 路径**（热路径只有 memcpy/memcmp）：
```
用户态 bench-mmap:
    get_info()
    mmap()             ←── 只做一次
    for i in iter:
        fill_pattern()
        memcpy(dst, src, len)
        memcmp(src, dst, len)
    munmap()            ←── 只做一次
```

### 6.2 perf stat 对比预期

| 指标 | baseline | optimized | 说明 |
|------|----------|-----------|------|
| task-clock | 高 | 低 | mmap 路径耗时占比 |
| context-switches | 高 | 低 | munmap syscall 切换 |
| page-faults | 高 | 低 | VMA 创建/销毁触发 |

---

## 七、关键数据解读

### 7.1 records/day32-local-001 的核心数据

```
baseline avg_us=283.590
optimized avg_us=0.892

avg_latency_gain_pct=99.65
p99_latency_gain_pct=99.85
throughput_gain_pct=24826.37
```

### 7.2 收益计算

```
latency_reduction = (baseline_avg - optimized_avg) / baseline_avg
                 = (283.590 - 0.892) / 283.590
                 = 282.698 / 283.590
                 = 99.65%

throughput_gain = (optimized_tp - baseline_tp) / baseline_tp * 100
                = (1575.094 - 6.585) / 6.585 * 100
                = 24826.37%
```

### 7.3 为什么优化效果如此显著？

```
baseline 每次迭代的成本：
  - 2 个 syscall（mmap + munmap）
  - 2 个 VMA 创建/销毁
  - 大量内核锁操作

optimized 每次迭代的成本：
  - 只有 memcpy + memcmp（纯用户态，无 syscall）

当 len=256 字节时：
  - memcpy 256 字节 ≈ 0.05 微秒
  - mmap + munmap ≈ 280 微秒

所以优化后提升 99.65% 是合理的。
```

---

## 八、与 Day33 的关系

| 特性 | Day32 | Day33 |
|------|-------|-------|
| 主题 | perf/ftrace 热点分析 | ftrace 函数调用链 |
| 工具 | perf stat/record/report | function_graph |
| 目标 | 找到热点函数 | 看清调用路径 |
| 改进对象 | mmap 复用 | 待定 |

---

## 九、验收标准

### 9.1 必须满足

- `mmap-verify` 返回 `verify_ok=1`
- `bench-mmap-baseline` 有有效统计输出
- `bench-mmap`（optimized）有有效统计输出
- `compare-mmap` 显示优化收益
- `irq_delta == 2`（DMA 往返正常）

### 9.2 关键证据

```
compare-mmap.txt:
  avg_latency_gain_pct=99.65     ← mmap 复用显著有效
  p99_latency_gain_pct=99.85
  throughput_gain_pct=24826.37   ← 吞吐量提升 248 倍
  irq_delta=2

dmesg-driver.txt:
  dma_alloc_coherent ok
  probe success
```

### 9.3 perf 补充证据（如执行）

```
host-perf-baseline.report.txt:
  mmap / munmap 相关函数占 CPU 高百分比

host-perf-optimized.report.txt:
  memcpy / memcmp 占 CPU 高百分比
  mmap / munmap 热点显著减少
```

---

## 十、面试要会讲的五句话

1. **"Day32 的核心是把 Day31 的纯测量升级为一次最小优化闭环"**
   → Day31 测量三条路径的基准，Day32 围绕 mmap 复用做一次有证据的优化

2. **"mmap munmap 的成本主要来自 VMA 创建/销毁和 syscall 开销"**
   → 每次 mmap+munmap 循环涉及两次 syscall、两个 VMA 操作、页表更新、内核锁竞争

3. **"optimized 路径把 mmap 准备从热路径移除，实现 99.65% 延迟降低"**
   → 循环内只保留 memcpy/memcmp，循环外只做一次 mmap/munmap

4. **"perf stat 看到 task-clock 和 context-switches 显著下降是优化有效的证据"**
   → 说明内核态时间减少，CPU 更专注在用户态的计算上

5. **"Day32 的优化点很小但证据链很完整，适合作为性能优化的最小范式"**
   → 先用 perf 找到热点，再做最小改动，最后用 records 证明效果
