# 04_DEEP_LEARNING — MSI-X 核心知识点（软教学版）

## MSI-X 核心概念（软模拟版）

### 什么是 MSI-X？

MSI-X（Message Signaled Interrupts - Extended）是 PCI-e 设备使用的一种中断机制。与传统 INTx 中断相比：

| 特性 | INTx (legacy) | MSI | MSI-X |
|------|---------------|-----|-------|
| 中断数 | 1（共享） | 1~32 | 1~2048 |
| CPU 亲和性 | 不可绑 | 可绑 | 每向量独立绑定 |
| 共享 | 多个设备共享 | 单设备独享 | 向量间完全独立 |

**MSI-X 的核心优势**：每个向量（vector）可以独立绑定到不同 CPU，实现真正的多核并行中断处理。

### 软模型如何模拟 MSI-X

在 soft 版本中，真实硬件被替换为软件数据结构：

| 真实 MSI-X 概念 | soft 教学模拟 |
|----------------|--------------|
| `pci_alloc_irq_vectors()` | `struct stage10_vector vectors[N]` |
| MSI-X vector 编号 | `vector_id`（等于 qid） |
| 向量绑定 CPU | `target_cpu` + `queue_work_on(target_cpu)` |
| MSI handler | `irq_workfn()` |
| 触发 MSI | `queue_work(irq_wq, &q->irq_work)` |
| /proc/interrupts | `debugfs/vectors` |

## irq_work 语义

`irq_work` 是内核提供的一种机制，允许在任意上下文中调度"尽力而为"的回调：

```c
struct irq_work {
    struct __call_single_node node;
    void (*func)(struct irq_work *);
};
```

我们的模拟：
```c
INIT_WORK(&q->irq_work, stage10_irq_workfn);  // 等价于 request_threaded_irq

// 触发"中断"（调度 irq_work）
queue_work_on(target_cpu, irq_wq, &q->irq_work);
```

对比真实 MSI-X：
```c
// 真实 PCI 版本：写 BAR 触发硬件 MSI
writel(1, bar + qid * 8);

// 软版本：调度 irq_work
queue_work_on(target_cpu, irq_wq, &q->irq_work);
```

## 为什么需要 irq_workfn ？

直接调用 `napi_schedule()`（stage09 方式）的问题：
- 缺少"中断语义"：无法区分 raise/handle/schedule 三个阶段
- 无法模拟 CPU 亲和性：无法指定在某 CPU 上处理

引入 irq_workfn 后：
- `raise_irq`：记录 raise 时间戳，调度 irq_work
- `irq_workfn`：在目标 CPU 上执行，记录 handle 时间戳，调用 napi_schedule
- `napi_poll`：实际处理

这模拟了真实硬件 MSI 的完整语义链：**硬件 raise → CPU 接收到中断 → 中断 handler → NAPI poll**。

## target_cpu 亲和性

`stage10_pick_irq_cpu(qid)` 用 round-robin 将向量分布到不同 CPU：

```
qid=0 → CPU0, qid=1 → CPU1, qid=2 → CPU2, ...
```

这样每个 CPU 独立处理一个队列的 TX/RX 中断，避免跨核锁竞争——这正是现代高性能 NIC 的设计目标。

## 与真实 PCI 版本的对比

学习完 soft 版本后，理解真实 PCI 版本只需要替换底座：

| 概念层 | soft | pci |
|--------|------|-----|
| 中断触发 | `queue_work_on(target_cpu)` | `writel(BAR + qid*8)` → 硬件 MSI |
| 向量分配 | 数组 | `pci_alloc_irq_vectors()` |
| 中断处理 | `irq_workfn()` | `request_threaded_irq()` 的 handler |
| NAPI 调度 | `__napi_schedule()` | 同上（通用接口） |
| CPU 亲和性 | `queue_work_on(target_cpu)` | `/sys/irq/<N>/smp_affinity` |

**上层语义完全一致**：TX 路径、异步 backend、NAPI poll、消费 RX 帧——这些代码在两个版本中是相同的。
