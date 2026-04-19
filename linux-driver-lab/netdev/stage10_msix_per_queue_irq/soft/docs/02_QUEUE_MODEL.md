# 02_QUEUE_MODEL — 队列模型与向量调度

## Per-queue 向量架构

stage10 soft 在 stage09 per-queue NAPI/backend 基础上，给每个队列分配一个 `struct stage10_vector`：

```
struct stage10_vector {
    u16 vector_id;          /* 向量编号（等于 qid） */
    u16 qid;                /* 对应的队列 ID */
    int target_cpu;         /* 亲和性目标 CPU */
    atomic64_t raise_count; /* raise_irq() 调用次数 */
    atomic64_t handle_count;/* irq_workfn 执行次数 */
    atomic64_t schedule_count; /* napi_schedule 调用次数 */
    char name[32];
};
```

## 向量分配

在 `stage10_soft_init()` 中，初始化 num_queues 个 vector：

```c
for (i = 0; i < priv->num_queues; i++) {
    priv->vectors[i].vector_id = i;
    priv->vectors[i].qid = i;
    priv->vectors[i].target_cpu = stage10_pick_irq_cpu(i);
}
```

CPU 选择用 round-robin 分配：
```c
static int stage10_pick_irq_cpu(u16 qid)
{
    int want = qid % num_online_cpus();
    int idx = 0, cpu;
    for_each_online_cpu(cpu) {
        if (idx == want) return cpu;
        idx++;
    }
    return raw_smp_processor_id();
}
```

## irq_workfn — 模拟 MSI handler

```c
static void stage10_irq_workfn(struct work_struct *work)
{
    struct stage10_queue *q = container_of(work, struct stage10_queue, irq_work);
    struct stage10_priv *priv = q->priv;
    struct stage10_vector *vec = &priv->vectors[q->qid];
    unsigned long flags;

    vec->last_raise_ns = stage10_now_ns();
    atomic64_inc(&vec->raise_count);
    smp_mb();

    if (!napi_schedule_prep(&q->napi)) {
        atomic64_inc(&q->stats.irq_masked);
        return;
    }
    __napi_schedule(&q->napi);
    atomic64_inc(&vec->schedule_count);
    vec->last_handle_ns = stage10_now_ns();
}
```

对比真实 MSI-X handler：
- 真实：`irq_handler() → napi_schedule()`
- soft：`irq_workfn() → napi_schedule_prep() + __napi_schedule()`

## raise_irq — 触发向量中断

```c
static void stage10_raise_irq(struct stage10_queue *q)
{
    struct stage10_priv *priv = q->priv;
    struct stage10_vector *vec = &priv->vectors[q->qid];

    atomic64_inc(&vec->raise_count);
    q->timeline.last_irq_ns = stage10_now_ns();
    vec->last_raise_cpu = raw_smp_processor_id();
    queue_work_on(vec->target_cpu, priv->irq_wq, &q->irq_work);
}
```

`queue_work_on(target_cpu)` 模拟了 MSI-X 的 CPU 亲和性——每个向量被调度到特定 CPU 上处理。

## Doorbell → 向量 → NAPI 链路

```
backend_workfn():
    ... 处理 TX/RX ...
    stage10_raise_irq(q)
      → queue_work_on(target_cpu, irq_wq, &q->irq_work)
         ↓ [work 在目标 CPU 上执行]
         irq_workfn()
           → napi_schedule_prep(&q->napi)
           → __napi_schedule(&q->napi)
              ↓
              napi_poll() → TX complete / RX consume
```

## Bounce Buffer（替代 DMA）

soft 版本不使用真实 DMA，使用 bounce buffer：

- TX：`kmalloc()` 分配 bounce buffer → `memcpy(skb->data, buf)` → 后端处理完 `kfree(buf)`
- RX：直接使用 `netdev_alloc_skb()`（不需要 DMA 映射）

这使得纯软模型可以在任何 Linux 环境运行，无需 PCI 或 DMA 支持。
