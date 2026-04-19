# 02_QUEUE_MODEL — 队列模型与 MSI-X 分配

## Per-queue MSI-X 架构

stage10 pci 在 stage09 per-queue NAPI/backend 基础上，给每个队列分配独立 MSI-X vector：

```
struct stage10_queue {
    unsigned int msix_vector;     // MSI-X vector 编号（0..N-1）
    int irq;                     // Linux IRQ 编号（由 pci_irq_vector() 获取）
    struct napi_struct napi;     // 绑定到此 vector 的 NAPI
    struct work_struct backend_work;
    ...
};
```

## MSI-X 分配流程

### 1. 启用 PCI 设备

```c
pci_enable_device(pdev);
pci_set_master(pdev);           // 使能总线主访问（DMA 需要）
```

### 2. 请求 MSI-X vectors

```c
int num_vectors = pci_alloc_irq_vectors(pdev,
                                        num_queues,   // min
                                        num_queues,   // max
                                        PCI_IRQ_MSIX);
if (num_vectors < 0) {
    dev_err(&pdev->dev, "failed to alloc MSI-X vectors\n");
    return num_vectors;
}
```

- `num_vectors` 返回实际分配的向量数（通常等于 `num_queues`）
- 每个 vector 独立，可绑定不同 CPU

### 3. 请求 IRQ

```c
for (i = 0; i < num_queues; i++) {
    int irq = pci_irq_vector(pdev, i);  // 获取第 i 个 vector 对应的 IRQ 号
    ret = request_threaded_irq(irq,
                               stage10_msix_handler,  // 顶部中断 handler
                               stage10_msix_thread,   // 底部 threaded handler
                               IRQF_SHARED,
                               DRV_NAME,
                               &queues[i]);
}
```

## MSI-X Handler

```c
static irqreturn_t stage10_msix_handler(int irq, void *data)
{
    struct stage10_queue *q = data;
    q->irq_count++;
    return IRQ_WAKE_THREAD;   // 唤醒底部线程
}

static irqreturn_t stage10_msix_thread(int irq, void *data)
{
    struct stage10_queue *q = data;
    napi_schedule(&q->napi);
    return IRQ_HANDLED;
}
```

注意：真实 PCI 驱动通常用 `request_threaded_irq`，handler 在硬中断上下文执行，thread_fn 在软中断上下文执行。stage10 中 handler 只做计数，thread_fn 做 napi_schedule。

## BAR Doorbell Register

PCI BAR（Base Address Register）是 PCI 配置空间中的寄存器，用于将设备内存映射到 CPU 地址空间：

```
QEMU PCI 配置空间：
  BAR0: 映射 doorbell registers
         offset +0x00 → q0 doorbell
         offset +0x08 → q1 doorbell
         ...
         每个 doorbell 占 8 bytes（一个 32-bit register）
```

驱动中获取 BAR：
```c
void __iomem *doorbell_bar = pci_iomap(pdev, 0, 0);
if (!doorbell_bar) {
    dev_err(&pdev->dev, "failed to iomap BAR0\n");
    return -ENOMEM;
}
```

触发 MSI（doorbell）：
```c
writel(1, doorbell_bar + qid * 8);   // 向 qid 对应的 doorbell 写 1
```

QEMU 的虚拟 PCI 设备捕获到这个写操作后，注入 MSI 中断到虚拟机。

## Doorbell → MSI → NAPI 链路

```
backend_workfn():
    ...
    writel(1, doorbell_bar + qid * 8)   ← 写 BAR doorbell
         ↓
    QEMU 注入 MSI 中断
         ↓
    stage10_msix_handler(irq, q)           ← 顶部 handler（hardirq）
         ↓
    napi_schedule(&q->napi)              ← 唤醒 NAPI
         ↓
    stage10_napi_poll()                   ← 处理 TX complete / RX consume
```

## 多队列与向量绑定

每个队列的 MSI-X vector 独立，可以通过 `/sys/irq/<IRQ>/smp_affinity` 调节 CPU 亲和性：

```bash
# 查看 q0 IRQ 的 affinity
cat /sys/irq/58/smp_affinity

# 将 q0 IRQ 绑定到 CPU 1
echo 1 > /sys/irq/58/smp_affinity

# 验证 /proc/interrupts 中断计数变化
watch -n1 cat /proc/interrupts | grep stage10
```

这实现了真实 NIC 的 per-queue CPU 亲和性——每个 CPU 处理独立队列的收发中断，无跨核锁竞争。

## probe/remove 对称性

```c
static int stage10_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    pci_enable_device(pdev);
    pci_set_master(pdev);
    pci_alloc_irq_vectors(pdev, ...);
    pci_iomap(pdev, 0, 0);           // 映射 BAR

    for (i = 0; i < num_queues; i++)
        request_threaded_irq(pci_irq_vector(pdev, i), ...);

    register_netdev(ndev);
}

static void stage10_pci_remove(struct pci_dev *pdev)
{
    for (i = 0; i < num_queues; i++)
        free_irq(pci_irq_vector(pdev, i), ...);   // 逆序释放

    pci_free_irq_vectors(pdev);
    pci_iounmap(pdev, doorbell_bar);
    unregister_netdev(ndev);
    pci_disable_device(pdev);
}
```
