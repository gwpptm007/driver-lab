本目录用于保存 stage10 的 smoke / MSI-X IRQ / queue distribution / timeline 实测记录。

## 记录列表

| 目录 | 日期 | 说明 |
|------|------|------|
| `20260419-215248-stage10-soft-smoke/` | 2026-04-19 | 全部 PASS（queue_dist/vector_check 均用增量检测） |
| `20260419-214307-stage10-soft-smoke/` | 2026-04-19 | soft 版本 smoke test，**全部 PASS**（queue_dist 修复后，q0+q1 均有流量） |
| `20260419-214101-stage10-soft-smoke/` | 2026-04-19 | queue_dist 增量检测修复验证，q0+33, q1+31 |
| `20260419-201342-stage10-soft-smoke/` | 2026-04-19 | soft 版本 smoke test，**全部 PASS**（test_rx bug 确认） |
| `20260419-182440-stage10-soft-smoke/` | 2026-04-19 | soft 版本首次 smoke test，**全部 PASS** |

---

## 整体架构：两种实现对比

stage10 包含两种实现，核心学习目标相同，但运行环境要求不同：

| 维度 | `soft/` | `pci/` |
|------|---------|---------|
| 架构 | `alloc_etherdev_mqs()` 纯软 netdev | `pci_driver` + 真实 PCI 设备 |
| MSI-X | `struct stage10_vector` + `irq_work` 模拟 | `pci_alloc_irq_vectors()` + `request_threaded_irq()` |
| 中断底座 | `queue_work_on(target_cpu)` | 真实 MSI-X hardware interrupt |
| doorbell | `queue_work()` | `writel(BAR + qid*8)` 写 PCI BAR |
| 测试环境 | 任何 Linux VM（VMware 可用） | 需要嵌套虚拟化（KVM）或真实 PCI 硬件 |
| `/proc/interrupts` | 不可观测 | 可观测 `stage10-q0`, `stage10-q1` 等 IRQ |
| `debugfs/vectors` | 可观测（软模拟） | 可观测（真实 MSI-X） |

**核心流程（两种实现共享）**：

```
TX:  ndo_start_xmit → mark_doorbell → backend_workfn → raise_irq(vector) → napi_poll
RX:  backend_workfn produce → raise_irq → napi_poll → netif_receive_skb
```

---

## soft/ — 纯软教学模型（当前可测试）

### 测试方法

```bash
# 在 VMware 虚拟机中
cd linux-driver-lab/netdev/stage10_msix_per_queue_irq/soft
./scripts/build.sh        # 编译驱动和工具
./scripts/smoke.sh        # 完整 smoke test
```

### 通过标准

- `vector_check`：2+ vectors 的 `handle >= 1`（vector 中断被处理）
- `queue_dist_check`：2+ 队列 `tx_submit > 0`（多队列分发成立）
- `timeline_check`：`doorbell_to_backend_ns > 0`（异步链路成立）

### 已知限制

- soft 版本使用 bounce buffer 代替真实 DMA（因为 `alloc_etherdev_mqs` 没有 DMA 设备底座）
- 不需要理解这个限制才能学习 MSI-X 向量调度语义

---

## pci/ — 真实 PCI + MSI-X（当前不可测试）

### 当前状态

**不可测试**——VMware Workstation 不支持嵌套 KVM，无法在当前环境中运行。

### 测试要求

**方案 A：嵌套虚拟化（KVM on KVM）**

需要一台支持嵌套虚拟化的物理机，或配置 VMware 支持 `hypervisor盖**:

```bash
# 在支持嵌套 VT-x/AMD-V 的物理机上
# VMware 设置：Processors → "Virtualize Intel VT-x/EPT or AMD-V/RVI"
# 或者使用 KVM 宿主直接运行
```

**方案 B：QEMU 裸机测试**

```bash
cd linux-driver-lab/netdev/stage10_msix_per_queue_irq/pci
./scripts/build.sh
./scripts/run.sh reload      # 启动 QEMU + 加载模块
./scripts/smoke.sh           # 完整 smoke test
./scripts/irq_check.sh       # 验证 /proc/interrupts
```

**方案 C：真实 PCI 硬件**

需要一块支持 MSI-X 的 PCI 网卡或开发板。

### 验证方法

```bash
# 启动 QEMU 后
./scripts/smoke.sh           # 完整 smoke test

# 验证真实 MSI-X 中断
cat /proc/interrupts | grep stage10
# 期望看到：
#   stage10-q0: [IRQ number] ... ...
#   stage10-q1: [IRQ number] ... ...

# 验证 IRQ affinity
cat /sys/irq/<IRQ>/smp_affinity
echo 4 > /sys/irq/<IRQ>/smp_affinity  # 迁移到 CPU 3
```

### 通过标准

- `/proc/interrupts` 中有 `stage10-q0`, `stage10-q1` 等 IRQ 条目
- `irq_count > 0`（MSI 中断被触发）
- `vector_check` 同 soft 版本

---

## 各子测试验证方法

### 1. smoke test

```bash
cd linux-driver-lab/netdev/stage10_msix_per_queue_irq/soft
./scripts/smoke.sh
```

生成 `records/<timestamp>-stage10-soft-smoke/` 目录。

### 2. MSI-X vector 验证（soft 版本）

```bash
./scripts/vector_check.sh records/<timestamp>
```

**通过标准**：至少 2 个 vector 的 `handle_count >= 1`。

### 3. 多队列分布验证

```bash
./scripts/queue_dist_check.sh records/<timestamp>
```

**通过标准**：至少 2 个队列有 `tx_submit > 0`。

### 4. 异步链路验证

```bash
./scripts/timeline_check.sh records/<timestamp>
```

**通过标准**：至少 1 个队列 `doorbell_to_backend_ns > 0`。

---

## 典型成功输出

### 2026-04-19 soft 版本 — 全部 PASS

```
=== queue_dist ===
queue_dist PASSED: 2 queues with tx_submit > 0
  tx_submit values: 10
68
=== vector_check ===
vector check PASSED: 2 vectors with handle_count >= 1
=== timeline ===
timeline PASSED: 2 queue(s) with doorbell_to_backend_ns > 0
q0: submit_to_doorbell_ns=121 doorbell_to_backend_ns=30071 backend_to_irq_ns=12455 irq_to_poll_ns=3875
q1: submit_to_doorbell_ns=270 doorbell_to_backend_ns=991 backend_to_irq_ns=480 irq_to_poll_ns=1092
```

**关键数据解读**：
- `submit_to_doorbell_ns`：TX 路径从 `ndo_start_xmit` 到 `mark_doorbell` 的延迟（~121ns）
- `doorbell_to_backend_ns`：doorbell 到 backend workfn 执行的延迟（~30µs，包含工作队列调度）
- `backend_to_irq_ns`：backend 处理完到 IRQ handler 执行的延迟（~12µs）
- `irq_to_poll_ns`：IRQ handler 到 NAPI poll 执行的延迟（~3.8µs）
- 完整的异步路径：TX submit → doorbell → backend work → raise_irq → napi_poll → TX complete

---

## 测试环境

- **测试机**：VMware Workstation 虚拟机，Ubuntu 24.04，kernel 6.8.0-107-generic
- **远程地址**：192.168.65.135（wq7）
- **接口**：`nds10s`（soft 版本），2 队列，ring_size=128
- **驱动版本**：netdev_stage10_soft（bounce buffer 无 DMA 模式）

---

## 历史修复记录

### 2026-04-19 — soft 驱动 DMA 问题修复

**问题**：TX 帧全部 drop（`tx_drop` 持续增长），`dma_map_single` 在虚拟 netdev 上失败。

**根因**：`alloc_etherdev_mqs` 创建的纯软 netdev 没有真实 DMA 设备底座，`dma_set_mask_and_coherent` 无法真正解决 VMware 虚拟 IOMMU 的限制。

**修复**：
1. 移除所有 `dma_map_single` / `dma_unmap_single` 调用
2. TX 路径改用 `kmalloc` bounce buffer + `memcpy`
3. RX 路径直接使用 `netdev_alloc_skb`（无需 DMA 映射）
4. backend 处理完 `kfree(buf)` 释放 bounce buffer

**同时修复**：`send_stage10_frame.c` 中 `memset(frame, ...)` 覆盖 `ifr.ifr_ifindex` 的 bug，改用 `if_nametoindex()`。

### 2026-04-19 — test_rx 统计不可信（已知 bug）

**现象**：smoke test 中 `q1 test_tx=64` 且 `rx_consume delta=64`，但 `test_rx=0`。

**根因**：`stage10_consume_rx_one()` 里 `eth_type_trans()` 会 `skb_pull(ETH_HLEN)`，之后 `skb->data` 已跳过 Ethernet header。而 `stage10_is_test_frame()` 在 `skb->data + ETH_HLEN` 处查 magic —— 这实际上是 **Ethernet header 之后 28 字节处**，而 magic 在 **Ethernet header 之后 14 字节处**。

两种修复路径：
1. `eth_type_trans()` 前检查：保存 `skb->data` 位置，在 pull 之前查 magic
2. `eth_type_trans()` 后检查：直接用 `skb->data`（已跳过 header）查 magic，不再 + ETH_HLEN

**结论**：`test_rx` 统计当前不可信，soft/pci 版本均有此问题。

### 2026-04-19 — queue_dist 检测修复 + send 帧格式改进

**问题 1**：queue_dist_check 只检查 `tx_submit > 0`（总量），不检查增量。导致历史残留流量让检查假 PASS。

**修复**：
1. `queue_dist_check.sh`：改为检查 `before/after` 增量
2. `send_stage10_frame.c`：改用 ETH_P_IP + 变化源 IP（`192.168.100.1+i`），让 `skb_tx_hash()` 对每帧产生不同 hash，实现真正的多队列分散
3. `recv_stage10_frame.c`：匹配新帧格式（ETH_P_IP + IP-proto=253）

**验证结果**（20260419-214307）：
- q0 tx_submit delta: +33
- q1 tx_submit delta: +31
- vector_check: 2 vectors ✅
- timeline: async链路成立 ✅

### 2026-04-18 — stage09 smoke test PASS

stage09（无 MSI-X，纯 soft IRQ）在 VMware 虚拟机上正常工作。
