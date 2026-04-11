# Day31 深度学习指南 - Bench 性能基准测试

## 一、Day31 是什么？

Day31 是 W5 的第三天，承接 Day30 的 mmap 零拷贝链路，引入 **性能基准测试（Bench）**。

**核心目标**：量化 `ioctl / mmap / dma` 三条路径的延迟、吞吐和 CPU 占用，建立可重复的性能基准。

---

## 二、Day30 → Day31 的核心变化

### 2.1 功能 vs 性能

| 维度 | Day30 | Day31 |
|------|-------|-------|
| 目标 | 验证 mmap 零拷贝能工作 | 测量这链路有多快/多稳 |
| 数据路径 | 一次性验证 | 多次迭代统计 |
| 输出 | verify_ok=1 | avg_us, p50/p95/p99, throughput_mbps |
| 主角 | 功能正确性 | 性能可量化 |

### 2.2 新增度量指标

**延迟指标**：
- `last_run_ns`：最近一次 DMA 运行的**内核视角**耗时（纳秒）
- `avg_us`：平均延迟（微秒）
- `p50/p95/p99_us`：分位数延迟

**吞吐指标**：
- `throughput_mbps`：兆比特/秒

**CPU 占用**：
- `cpu_user_pct`：用户态 CPU 占比
- `cpu_sys_pct`：内核态 CPU 占比

**成功率**：
- `total_run_calls`：总运行次数
- `total_run_ok / total_run_fail`：成功/失败次数

---

## 三、三条被测路径

### 路径 A：`ioctl` 控制路径

**测试内容**：`DAY31_IOC_GET_INFO`（纯控制，不搬数据）

**意义**：衡量"用户态 → 内核态 → 返回"的控制开销基线

**典型结果**：`avg_us ≈ 16μs`

### 路径 B：`mmap` 用户态路径

**测试内容**：
1. 用户态写 src pattern
2. 用户态清 dst
3. 用户态 `memcpy(dst, src, len)`
4. 用户态 `memcmp(src, dst, len)`

**意义**：衡量"纯用户态直接访问 mmap buffer"的速度（无设备参与）

**典型结果**：`avg_us ≈ 0.5μs`，`throughput_mbps ≈ 887MB/s`

### 路径 C：DMA 端到端路径

**测试内容**：
1. 用户态写 src pattern（mmap）
2. 用户态清 dst（mmap）
3. `ioctl(RUN_DMA)` 触发两段 DMA
4. 用户态 `memcmp(src, dst, len)`

**意义**：衡量"零拷贝 mmap + EDU DMA 往返"的完整路径

**典型结果**：`avg_us ≈ 200ms`（EDU 模拟的 DMA 比真实硬件慢很多）

---

## 四、内核新增：ktime_get_ns()

### 4.1 什么是 ktime？

```c
#include <linux/ktime.h>

u64 start_ns = ktime_get_ns();
// ... 执行 DMA ...
u64 end_ns = ktime_get_ns();
u64 elapsed_ns = end_ns - start_ns;
```

`ktime_get_ns()` 返回自系统启动以来的纳秒数，用于高精度计时。

### 4.2 为什么用纳秒？

```
微秒（μs）：10^-6 秒
纳秒（ns）：10^-9 秒

DMA 一次往返约 200μs = 200,000ns
用纳秒可以更精确地测量
```

---

## 五、Day31 结构体新增字段

### 5.1 day31_dev 新增 bench 统计字段

```c
struct day31_dev {
    /* === 与 Day30 相同 === */
    struct pci_dev *pdev;
    void __iomem *bar0;
    // ... 中断资源 ...
    // ... DMA 资源 ...
    // ... mmap 结果 ...

    /* === Day31 新增：最小 bench 统计 === */
    u64 total_run_calls;    /* RUN_DMA 调用总次数 */
    u64 total_run_ok;       /* 成功次数 */
    u64 total_run_fail;     /* 失败次数 */
    u64 last_run_ns;        /* 最近一次运行耗时（纳秒）*/
};
```

### 5.2 为什么需要在驱动里统计？

```
用户态 bench 工具统计的是"端到端"耗时
包括：用户态 memcpy + ioctl syscall + 内核 DMA + 返回

但 last_run_ns 统计的是"内核视角"的纯 DMA 耗时
驱动知道 DMA 何时开始、何时结束

两者结合才能完整分析性能瓶颈
```

---

## 六、bench_verbose 模块参数

### 6.1 为什么需要这个参数？

```
bench-dma 每次运行触发两次 IRQ
如果每次 IRQ 都 dev_info() 打印
在 -nographic QEMU 下会显著拖慢自动化
```

### 6.2 使用方式

```bash
# 默认关闭（自动化推荐）
insmod day31_edu_bench.ko

# 开启 verbose（调试用）
insmod day31_edu_bench.ko bench_verbose=1
```

### 6.3 实现原理

```c
static bool bench_verbose;
module_param(bench_verbose, bool, 0644);

// IRQ handler 中：
if (unlikely(bench_verbose))
    dev_info(&d->pdev->dev, "irq handler: ...");
```

---

## 七、Bench 工具统计口径

### 7.1 分位数计算

```c
// 样本数组排序后：
// p50 = arr[n * 0.50]
// p95 = arr[n * 0.95]
// p99 = arr[n * 0.99]

// 采用"按索引取样"而非插值
// 目的是让 day31 records 易读、可复现
```

### 7.2 吞吐计算

```c
throughput_mbps = (ok_ops * payload_bytes * 8.0) / wall_total_us
// ok_ops：成功操作数
// payload_bytes：每次传输的字节数
// 乘以 8 转换成比特
// 单位：兆比特/秒（Mbps）
```

### 7.3 CPU 占用计算

```c
// 通过 getrusage() 获取进程资源使用
// user_us = ru_end.ru_utime - ru_begin.ru_utime
// sys_us = ru_end.ru_stime - ru_begin.ru_stime

cpu_user_pct = (user_us / wall_total_us) * 100.0
cpu_sys_pct = (sys_us / wall_total_us) * 100.0
```

---

## 八、完整调用链

### 8.1 bench-dma 完整流程

```
用户态 bench-tool:
    │
    ├─ 1. get_info()  获取 map_bytes/src_off/dst_off
    │
    ├─ 2. mmap()  映射 coherent DMA buffer
    │
    ├─ 3. 循环 iter+warmup 次:
    │     ├─ fill_pattern(src, len, seed+i)
    │     ├─ memset(dst, 0, len)
    │     ├─ t0 = mono_ns()
    │     ├─ ioctl(RUN_DMA)  ←─── 内核计时开始
    │     │       ↓
    │     │     day31_do_run_dma()
    │     │       ├→ ktime_get_ns() → start_ns
    │     │       ├→ program_dma(RAM→EDU)
    │     │       ├→ program_dma(EDU→RAM)
    │     │       ├→ ktime_get_ns() → end_ns
    │     │       └→ last_run_ns = end_ns - start_ns
    │     ├─ t1 = mono_ns()
    │     ├─ memcmp(src, dst, len)
    │     └─ 记录 sample
    │
    ├─ 4. 排序样本，计算分位数
    │
    ├─ 5. getrusage() 计算 CPU 占用
    │
    └─ 6. 输出 avg/p50/p95/p99/throughput/cpu_pct
```

---

## 九、三条路径的性能对比

### 9.1 典型数值（QEMU EDU 环境下）

| 路径 | avg_latency | throughput | CPU占用 | 说明 |
|------|-------------|------------|---------|------|
| ioctl | ~16μs | N/A | ~5% | 纯控制路径 |
| mmap | ~0.5μs | ~887MB/s | ~2% | 纯内存复制 |
| dma | ~200ms | ~0.08Mbps | ~10% | EDU模拟DMA很慢 |

### 9.2 为什么 DMA 这么慢？

```
QEMU EDU 是软件模拟的设备
每次 DMA 往返涉及：
  - 宿主机 QEMU 进程模拟
  - 虚拟 PCIe 事务
  - 虚拟 DMA 控制器

真实 PCIe DMA 硬件通常：
  - 延迟 < 1μs
  - 吞吐 > 1000MB/s
```

### 9.3 各路径的用途

```
ioctl 路径：衡量内核调用开销
mmap 路径：衡量零拷贝内存访问上限
dma 路径：衡量设备数据搬运实际性能
```

---

## 十、与 Day32 的关系

| 特性 | Day31 | Day32 |
|------|-------|-------|
| 主题 | Bench 基线 | perf/ftrace 性能分析 |
| 工具 | 手动 bench 工具 | 内核性能追踪 |
| 目标 | 建立可量化基准 | 找到热点函数 |

---

## 十一、验收标准

### 11.1 必须满足

- `mmap-verify` 返回 `verify_ok=1`
- `bench-ioctl` 有有效统计输出
- `bench-mmap` 有有效统计输出
- `bench-dma` 有有效统计输出
- `total_run_ok > 0`
- `irq_delta == 2`

### 11.2 关键证据

```
bench-dma.txt:
  success_ops=200
  failed_ops=0
  avg_us=200222.804    ← DMA 确实在跑
  p99_us=208327.968
  throughput_mbps=0.081
  irq_delta=2          ← 两段 DMA 各触发一次 IRQ

dmesg-driver.txt:
  dma_alloc_coherent ok: virt=... dma=... bytes=4096
  probe success
  (无 Oops/DMA error)
```

---

## 十二、面试要会讲的五句话

1. **"Day31 的核心是把 Day30 的功能测试升级为性能基准测试"**
   → Day30 验证 mmap 零拷贝能工作，Day31 测量它有多快、有多稳

2. **"last_run_ns 测的是内核视角的纯 DMA 耗时"**
   → 用户态计时包含 memcpy/ioctl syscall，last_run_ns 只包含 DMA 编程到完成

3. **"三条 bench 路径分别衡量不同阶段的性能"**
   → ioctl 衡量控制路径开销，mmap 衡量纯内存访问，dma 衡量设备数据搬运

4. **"bench_verbose=0 是为了避免 IRQ 日志拖慢自动化"**
   → 每次 DMA 触发两次 IRQ，频繁打印会显著影响 QEMU -nographic 性能

5. **"QEMU EDU 的 DMA 比真实硬件慢很多是正常的"**
   → 软件模拟的 DMA 没有硬件加速，200ms 级别是预期行为
