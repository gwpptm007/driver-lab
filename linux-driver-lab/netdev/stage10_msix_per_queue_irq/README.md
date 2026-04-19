# stage10 — MSI-X per-queue IRQ

本目录包含 stage10 的**两种实现**，分别对应不同的测试环境：

## 目录结构

```
stage10_msix_per_queue_irq/
├── soft/           # 纯软教学模型（可在任何 Linux 环境运行）
│   └── ...
└── pci/            # 真实 PCI + MSI-X 版本（需要嵌套 virt 或真实硬件）
    └── ...
```

## soft/ — 纯软教学模型

**无需 PCI、无需 QEMU、可在任何 Linux VM 中运行**

基于 `alloc_etherdev_mqs()` 纯软 netdev，用 `struct stage10_vector` + `irq_work` 模拟 MSI-X 语义：

- `stage10_vector`：每队列对应一个 vector，支持 `target_cpu` 亲和性分发
- `irq_workfn`：模拟 MSI 中断处理（raise → handle → napi_schedule）
- `backend_workfn`：TX/RX 处理后触发 `stage10_raise_irq()` → vector 调度
- `debugfs/vectors`：观测 vector→queue→CPU 映射和 handle_count

### 测试方法
```bash
cd soft
./scripts/build.sh          # 编译驱动和工具
./scripts/run.sh reload     # 加载模块
./scripts/smoke.sh          # 完整 smoke test
```

**通过标准**：
- `vector_check`: 2+ vectors 的 `handle >= 1`（vector 中断被处理）
- `queue_dist_check`: 2+ 队列 `tx_submit > 0`（多队列分发成立）
- `timeline_check`: `doorbell_to_backend_ns > 0`（异步链路成立）

## pci/ — 真实 PCI + MSI-X 版本

**需要嵌套虚拟化（KVM）或真实 PCI 硬件**

基于 `pci_driver` + `pci_alloc_irq_vectors()` + `request_threaded_irq()` 实现真实 MSI-X：

- 真实 PCI 设备探测（vendor=0x1D9B, device=0x1010）
- `pci_alloc_irq_vectors(num_queues, num_queues, PCI_IRQ_MSIX)`
- 真实 MSI-X 中断 handler（`stage10_msix_handler`）
- BAR doorbell register（`writel(BAR + qid*8)` 触发 MSI）
- `/proc/interrupts` 可观测真实 IRQ 编号

### 测试方法
```bash
cd pci
./scripts/build.sh          # 编译驱动和工具
./scripts/run.sh reload     # 启动 QEMU + 加载模块
./scripts/smoke.sh          # 完整 smoke test
./scripts/irq_check.sh      # 验证 /proc/interrupts 中有 stage10 条目
```

**通过标准**：
- `/proc/interrupts` 中有 `stage10-q0`, `stage10-q1` 等 IRQ 条目
- `irq_count > 0`（MSI 中断被触发）
- `vector_check` 同 soft 版本

## 两种实现的区别

| 维度 | soft/ | pci/ |
|------|-------|------|
| 架构 | `alloc_etherdev_mqs()` | `pci_driver` + `pci_enable_device()` |
| MSI-X | `struct stage10_vector` 模拟 | `pci_alloc_irq_vectors()` 真实分配 |
| doorbell | `queue_work_on(target_cpu)` | `writel(BAR + qid*8)` |
| 测试环境 | 任何 Linux VM | 需要嵌套 virt 或裸机 |
| /proc/interrupts | 不可观测 | 可观测真实 IRQ |
| affinity | 通过 CPU round-robin | `/sys/irq/<N>/smp_affinity` 可写 |

## 核心学习目标

两种实现共享相同的**上层语义**：
- per-queue NAPI + backend work
- TX: `ndo_start_xmit` → `mark_doorbell` → `backend_workfn` → `raise_irq` → `napi_poll`
- RX: `backend_workfn` produce → `raise_irq` → `napi_poll` consume → `netif_receive_skb`
- `debugfs/{stats,queues,timeline,vectors}` 可观测性

区别在于**中断底座**：软模拟（soft）vs 真实 MSI-X（pci）。