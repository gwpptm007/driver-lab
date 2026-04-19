# 03_ACCEPTANCE — 验收标准

## 通过标准

| 测试项 | 条件 | 验证方式 |
|--------|------|----------|
| 多队列分发 | 2+ 队列 `tx_submit > 0` | `queue_dist_check.sh` |
| MSI-X 中断 | `/proc/interrupts` 有 stage10-qN 条目 | `irq_check.sh` |
| 向量调度 | 2+ vectors `handle_count >= 1` | `vector_check.sh` |
| 异步链路 | 1+ 队列 `doorbell_to_backend_ns > 0` | `timeline_check.sh` |
| IRQ 触发 | `irq_count > 0` | `debugfs/irqs` |

## 运行测试

### 前提条件

**嵌套虚拟化环境**（VMware 不支持，需要以下之一）：
- 物理机 KVM
- VMware 开启 VT-x/EPT（某些 VMware 版本支持嵌套）
- QEMU 裸机

### 测试步骤

```bash
cd linux-driver-lab/netdev/stage10_msix_per_queue_irq/pci

# 1. 编译
./scripts/build.sh

# 2. 启动 QEMU 并加载模块
./scripts/run.sh reload

# 3. 运行 smoke test
./scripts/smoke.sh
```

## 手动验证

### 1. 检查 PCI 设备

```bash
lspci | grep -i stage10
# 预期输出：
# 00:02.0 Ethernet controller: Device 1d9b:1010
```

### 2. 检查 MSI-X 中断（核心验证）

```bash
cat /proc/interrupts | grep stage10
# 预期输出：
#   58:    123456   ivshmem  MSI  stage10-q0
#   59:     83420   ivshmem  MSI  stage10-q1
```

每个队列有独立的 IRQ 号（58, 59...），证明 MSI-X 向量分配成功。

### 3. 检查 debugfs

```bash
sudo cat /sys/kernel/debug/netdev_stage10/stats
sudo cat /sys/kernel/debug/netdev_stage10/irqs
sudo cat /sys/kernel/debug/netdev_stage10/vectors
sudo cat /sys/kernel/debug/netdev_stage10/timeline
```

预期：
- `stats`: tx_submit > 0（两个队列都有流量）
- `irqs`: 每个队列的 irq 编号和 irq_count
- `vectors`: 每个向量的 raise/handle/schedule 计数
- `timeline`: doorbell_to_backend_ns > 0（异步链路成立）

### 4. IRQ Affinity（可选）

```bash
# 查看 q0 的 affinity
cat /sys/irq/58/smp_affinity

# 将 q0 迁移到 CPU 1
echo 1 > /sys/irq/58/smp_affinity

# 验证 /proc/interrupts 中断计数是否在 CPU 1 上增长
watch -n1 cat /proc/interrupts | grep stage10
```

### 5. QEMU Monitor（调试用）

在 QEMU 运行期间，按 `Ctrl+a, c` 进入 monitor：

```
(qemu) info irq         # 查看中断状态
(qemu) info pci        # 查看 PCI 设备
(qemu) quit            # 退出 QEMU
```

## 关键指标解读

### timeline 数据

- `submit_to_doorbell_ns`：TX 路径从 `ndo_start_xmit` 到 `mark_doorbell` 的延迟
- `doorbell_to_backend_ns`：`mark_doorbell` 到 backend workfn 执行的延迟
- `backend_to_irq_ns`：backend 完成到 MSI handler 执行的延迟
- `irq_to_poll_ns`：MSI handler 到 NAPI poll 执行的延迟

### irqs 数据

```
q0: irq=58 vector=0 irq_count=71 affinity=0001
q1: irq=59 vector=1 irq_count=83 affinity=0002
```

每个队列的 `irq_count` 证明 MSI 中断被触发。

## 调试命令汇总

```bash
# PCI 设备
lspci -v | grep -A5 stage10

# MSI-X 向量数
cat /sys/bus/pci/devices/0000:00:02.0/msi_irqs

# 中断绑定
cat /proc/interrupts | grep stage10
watch -n1 cat /proc/interrupts | grep stage10

# debugfs
sudo cat /sys/kernel/debug/netdev_stage10/stats
sudo cat /sys/kernel/debug/netdev_stage10/irqs
sudo cat /sys/kernel/debug/netdev_stage10/vectors
sudo cat /sys/kernel/debug/netdev_stage10/timeline
sudo cat /sys/kernel/debug/netdev_stage10/queues

# dmesg
sudo dmesg | grep stage10
```
