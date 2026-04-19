# 04_DEEP_LEARNING — MSI-X 核心知识点

## MSI-X vs MSI vs INTx

| 特性 | INTx (legacy) | MSI | MSI-X |
|------|---------------|-----|-------|
| 中断数 | 1（共享） | 1~32 | 1~2048 |
| 共享方式 | 多个设备可共享 | 单设备独享 | 向量间完全独立 |
| CPU 亲和性 | 不可单独绑定 | 可绑定 | 每向量独立绑定 |
| 消息地址 | 无 | 厂商定义 | 表驱动（FHS） |
| 适用场景 | 传统设备 | 多核初普及 | 现代高性能 NIC |

### 为什么 MSI-X 优于 MSI？

1. **更多中断向量**：最多 2048 个（MSI 最多 32 个），足以支持现代 NIC 的多队列
2. **独立亲和性**：每个向量可绑定不同 CPU，消除跨核锁竞争
3. **表驱动**：MSI-X 使用独立的 Address/Data Table，不需要连续内存

## pci_alloc_irq_vectors 参数详解

```c
int pci_alloc_irq_vectors(struct pci_dev *dev,
                         unsigned int min_vecs,   // 最少需要向量数
                         unsigned int max_vecs,   // 最多申请向量数
                         unsigned int flags);     // PCI_IRQ_* 标志
```

### flags

| flag | 说明 |
|------|------|
| `PCI_IRQ_MSI` | 仅 MSI（不支持 MSI-X） |
| `PCI_IRQ_MSIX` | 仅 MSI-X |
| `PCI_IRQ_ALL_TYPES` | MSI 或 MSI-X 均可 |

### 返回值

- 成功：返回实际分配的向量数（介于 min 和 max 之间）
- 失败：返回负数 errno（`-ENOSPC` 表示向量不足）

### 典型用法

```c
int num_queues = 4;
int vectors = pci_alloc_irq_vectors(pdev, num_queues, num_queues, PCI_IRQ_MSIX);
if (vectors < 0) {
    dev_err(&pdev->dev, "MSI-X not available: %d\n", vectors);
    return vectors;
}
dev_info(&pdev->dev, "MSI-X vectors: %d\n", vectors);
```

## BAR Doorbell Register

### 什么是 BAR？

BAR（Base Address Register）是 PCI 配置空间中的寄存器，用于：
1. 声明设备需要的地址空间大小
2. 记录 CPU 地址空间中映射的起始地址
3. 驱动通过读写 BAR 地址与设备交互

### stage10 中的 BAR 布局

BAR0 被用作 doorbell registers：
```
BAR0 offset:
  +0x00: q0 doorbell  (32-bit RW)  — 写1触发 q0 的 MSI
  +0x08: q1 doorbell  (32-bit RW)  — 写1触发 q1 的 MSI
  +0x10: q2 doorbell  (32-bit RW)
  ...
  +0xN*8: qN doorbell
```

驱动中：
```c
void __iomem *doorbell_bar = pci_iomap(pdev, 0, 0);  // BAR0 = index 0

// 触发 qid 队列的中断
writel(1, doorbell_bar + qid * 8);
```

### 为什么用 8 bytes per queue？

- QEMU 的 ivshmem 设备使用 8-byte doorbell registers
- `writel(1, addr)` 写 32-bit 值
- 64-bit 系统自然 8-byte 对齐

## /proc/interrupts 中的 MSI 中断格式

```
  58:    123456   ivshmem  MSI  stage10-q0
  IRQ号   中断次数  设备名 类型   描述/共享
```

- Linux 将 MSI 和 MSI-X 都标记为 `MSI`（不区分）
- 向量数通过 IRQ 号区分（58 = vector 0, 59 = vector 1...）
- `ivshmem` 是 QEMU 提供的虚拟 PCI 设备类型

## MSI-X 与 NAPI 的协同

真实 NIC 的中断处理流程：

```
硬件 MSI-X (queue N)
  → CPU 接收中断（hardirq）
    → kernel IRQ handler（top-half）
      → ack hardware（如需要）
      → napi_schedule(&napi[N])
        → napi_poll() [softirq context]
          → TX completion processing
          → RX packet processing
```

MSI-X 让每个队列的 NAPI 在独立 CPU 上运行，实现真正的并行处理。

## IRQ Affinity

### smp_affinity

每个 IRQ 可以绑定到 CPU mask：

```bash
/sys/irq/<N>/smp_affinity    # 可读写，bitmask of CPUs
```

格式：十六进制 CPU mask（1 = CPU0, 2 = CPU1, 4 = CPU2...）

```bash
# 查看 IRQ 58 的 affinity
cat /sys/irq/58/smp_affinity
# 输出：00000001（绑定到 CPU 0）

# 绑定到 CPU 1
echo 2 > /sys/irq/58/smp_affinity

# 绑定到 CPU 0 和 CPU 2（multi-bit）
echo 5 > /sys/irq/58/smp_affinity
```

### 多队列 NIC 的 affinity 策略

现代 NIC（如 Intel ixgbe）的典型做法：
```
queue0 → IRQ 0 → CPU 0
queue1 → IRQ 1 → CPU 1
queue2 → IRQ 2 → CPU 2
...
```

每个 CPU 处理独立队列的 TX/RX 中断，无锁竞争。`irqbalance` 守护进程也可以自动优化 affinity。

## QEMU 中的 PCI 设备模拟

stage10 pci 版本依赖 QEMU 提供虚拟 PCI 设备。QEMU 命令行中的关键参数：

```
-device stage10_pci,addr=02.0,did=0x1010,vid=0x1D9B
```

- `stage10_pci`：QEMU 模拟的 PCI 设备类型
- `addr`：PCI 总线地址（bus.device.function）
- `did/vid`：device ID 和 vendor ID

QEMU 会：
1. 实现 PCI 配置空间（vendor ID, BAR registers...）
2. 捕获 doorbell register 写操作
3. 向虚拟机注入 MSI 中断

## 与 soft 版本的对比总结

| 概念层 | pci 版本 | soft 版本 |
|--------|---------|----------|
| 中断触发 | `writel(BAR + qid*8)` → 硬件 MSI | `queue_work_on(target_cpu)` |
| 向量分配 | `pci_alloc_irq_vectors()` | `struct stage10_vector[N]` |
| 中断处理 | `request_threaded_irq()` handler | `irq_workfn()` |
| CPU 亲和性 | `/sys/irq/<N>/smp_affinity`（真实） | `stage10_pick_irq_cpu()`（round-robin） |
| /proc/interrupts | 真实 IRQ 编号可见 | 不可观测 |
| 学习重点 | PCI/MSI-X 底层机制 | MSI-X 上层语义 |

**共同的上层语义**：TX 路径、异步 backend、NAPI poll、queue 模型——这些代码在两个版本中几乎完全一致。
