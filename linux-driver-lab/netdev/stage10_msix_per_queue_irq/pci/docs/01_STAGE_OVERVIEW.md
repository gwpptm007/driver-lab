# 01_STAGE_OVERVIEW — 架构总览

## 一句话定位

> stage10 pci 引入真实 PCI 设备 + MSI-X 中断，让每队列有独立硬件中断向量，实现真正的 per-CPU 向量亲和性。

---

## 核心目标

在 stage09 多队列模型的基础上，引入 PCI 总线框架和 MSI-X 中断机制：

- PCI device 注册（probe/remove）
- `pci_alloc_irq_vectors()` 分配 per-queue MSI-X vectors
- `request_threaded_irq()` 将每个 vector 绑定到对应 queue 的 NAPI context
- BAR register 作为 doorbell（替代 stage09 的 soft raise_irq）
- `/proc/interrupts` 中观测 per-queue IRQ 编号
- IRQ affinity 可通过 `/sys/irq/*/smp_affinity` 调节

---

## 与 stage09 的本质区别

| 维度 | stage09 | stage10 pci |
|------|---------|-------------|
| 中断方式 | 软件模拟（raise_irq → napi_schedule） | 真实 MSI-X hardware interrupt |
| 总线框架 | 无（纯 alloc_etherdev） | PCI subsystem（pci_driver） |
| doorbell | 内存标记（无硬件效果） | BAR register write → MSI → NAPI |
| IRQ 观测 | 不可观测 | `/proc/interrupts` + debugfs irqs |
| affinity | 不可调 | `/sys/irq/*/smp_affinity` 可写 |
| MSI 向量 | 无 | per-queue MSI-X vector |

---

## 架构图

```
                    QEMU PCI bus
                         │
          ┌──────────────▼──────────────┐
          │   stage10_pci (vendor=0x1D9B) │
          │                               │
          │   BAR 0: doorbell registers  │
          │   MSI-X: N vectors (N=队列数) │
          │                               │
          └──────────────┬───────────────┘
                         │
          ┌──────────────▼──────────────────┐
          │  struct stage10_priv            │
          │  struct pci_dev *pci_dev        │
          │  void __iomem *doorbell_bar     │
          │  num_vectors = N                 │
          │  queues[0..N-1]                  │
          └──────┬──────────────┬───────────┘
                 │              │
    ┌───────────▼──┐  ┌───────▼────────┐
    │  q0           │  │  q1             │
    │  msix_vec=0  │  │  msix_vec=1    │
    │  napi        │  │  napi          │
    │  backend_work│  │  backend_work  │
    │  IRQ 0──→NAPI│  │  IRQ 1──→NAPI │
    └──────────────┘  └────────────────┘
```

---

## 中断链路对比

**stage09（软中断）**：
```
xmit → mark_doorbell() → queue_work()
  backend_work → raise_irq()
    → napi_schedule(&q->napi)
```

**stage10 pci（真实 MSI-X）**：
```
xmit → mark_doorbell() → queue_work()
  backend_work → writel(BAR + qid*8)   ← 触发 MSI
    → MSI interrupt (IRQ N)
      → stage10_msix_handler()
        → napi_schedule(&q->napi)
```

---

## 关键数据结构

```c
struct stage10_priv {
    struct pci_dev *pci_dev;
    void __iomem *doorbell_bar;    // BAR0 映射的 doorbell 区域
    unsigned int num_vectors;       // MSI-X vector 数量
    struct stage10_queue *queues;
};

struct stage10_queue {
    unsigned int msix_vector;       // 对应的 MSI-X vector 编号
    struct napi_struct napi;        // 绑定到此 vector 的 NAPI
    struct work_struct backend_work;
    int irq;                        // Linux IRQ 编号
};
```

---

## 与 soft 版本的对应关系

| 概念 | pci 版本 | soft 版本 |
|------|---------|---------|
| 向量分配 | `pci_alloc_irq_vectors()` | `struct stage10_vector[N]` |
| 向量触发 | `writel(BAR + qid*8)` → MSI | `queue_work_on(target_cpu)` |
| 向量 handler | `request_threaded_irq()` | `INIT_WORK(&irq_work, irq_workfn)` |
| CPU 亲和性 | `/sys/irq/<N>/smp_affinity` | `stage10_pick_irq_cpu()` |
| /proc/interrupts | 真实 IRQ 可观测 | 不可观测 |
