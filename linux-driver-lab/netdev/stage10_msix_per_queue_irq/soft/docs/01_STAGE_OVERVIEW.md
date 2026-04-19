# 01_STAGE_OVERVIEW — 架构总览

## 一句话定位

> stage10 soft 是纯软教学模型，用 `struct stage10_vector` + `irq_work` 模拟 MSI-X 中断语义，无需真实 PCI 硬件即可学习 per-queue 向量调度的核心概念。

---

## 核心目标

在 stage09 多队列模型的基础上，用纯软方式模拟 MSI-X 的核心语义：

- `struct stage10_vector`：每队列对应一个"向量"，记录 raise/handle/schedule 计数
- `irq_workfn`：模拟 MSI 中断处理（raise → handle → napi_schedule）
- `queue_work_on(target_cpu, ...)`：模拟向量到 CPU 的亲和性分发
- `debugfs/vectors`：观测 vector→queue→CPU 映射和 handle_count
- 完整保留 stage09 的 per-queue NAPI + backend work 异步链路

---

## 与 stage09 的区别

| 维度 | stage09 | stage10 soft |
|------|---------|--------------|
| 向量语义 | 无 | `struct stage10_vector` 模拟 MSI-X |
| 中断处理 | `raise_irq()` → `napi_schedule()` | `irq_workfn` → `queue_work_on(target_cpu)` |
| CPU affinity | 不可控 | `stage10_pick_irq_cpu()` round-robin |
| debugfs | stats/queues/timeline | + vectors（新增） |
| 教学重点 | 多队列 + 异步 backend | MSI-X 向量调度语义 |

---

## 与 stage10 pci 的对应关系

| 真实 PCI 概念 | soft 模拟 |
|-------------|----------|
| `pci_alloc_irq_vectors()` | `struct stage10_vector[N]` 数组 |
| `request_threaded_irq()` | `INIT_WORK(&q->irq_work, irq_workfn)` |
| `writel(BAR + qid*8)` | `stage10_raise_irq(q)` → `queue_work_on(target_cpu)` |
| MSI handler 调用 `napi_schedule()` | `irq_workfn` 调用 `napi_schedule()` |
| `/proc/interrupts` 真实 IRQ 号 | `debugfs/vectors` 软模拟观测 |

---

## 架构图

```
ndo_start_xmit()
  → stage10_mark_doorbell()
    → queue_work(backend_wq, &q->backend_work)
    → stage10_raise_irq(q)
      → queue_work_on(vec->target_cpu, irq_wq, &q->irq_work)

irq_workfn()  [模拟 MSI handler]
  → napi_schedule_prep(&q->napi)
  → __napi_schedule(&q->napi)

backend_workfn()
  → TX/RX 处理
  → stage10_raise_irq(q)
```

---

## 中断链路

**stage09（软中断，直接调用 napi_schedule）**：
```
xmit → mark_doorbell() → queue_work()
  backend_work → raise_irq()
    → napi_schedule(&q->napi)
```

**stage10 soft（模拟 MSI-X，queue_work_on 到 target_cpu）**：
```
xmit → mark_doorbell() → queue_work()
  backend_work → stage10_raise_irq(q)
    → queue_work_on(target_cpu, irq_wq, &q->irq_work)
      → irq_workfn() → napi_schedule(&q->napi)
```

关键区别：`raise_irq` 不再直接调用 `napi_schedule`，而是调度 `irq_work`，再由 `irq_workfn` 调用 `napi_schedule`——这模拟了真实 MSI 中断的"raise → handler → napi"语义链。
