# Cross-Path Comparison: Kernel Netdev vs DPDK vs AF_XDP

## 1. 为什么需要这条对比

三条路径各自都有 evidence 和报告，但各自证明各自的能力。这篇对比的目标是回答一个问题：

> 三条路径的核心差异是什么？什么时候该用哪条？

这不是概念背诵，而是基于本项目真实实验数据做出的判断。

---

## 2. 核心差异：一张表说清楚

| 维度 | Kernel Netdev | DPDK | AF_XDP |
|------|--------------|------|--------|
| **包入口** | 驱动中断 → NAPI poll | PMD 轮询，绕过内核 | XDP hook → XSKMAP redirect |
| **内核参与** | 全程在内核（协议栈） | 零内核参与（用户态） | 仅 XDP hook 在内核，数据面在用户态 |
| **内存模型** | sk_buff（slab 分配） | rte_pktmbuf（hugepage 内存池） | UMEM frame（预分配共享内存） |
| **队列/环模型** | RX/TX ring（DMA 描述符环） | ethdev RX/TX queue | FILL / RX / TX / COMPLETION 四环 |
| **收包方式** | 中断驱动 + NAPI 轮询 | 用户态主动 rte_eth_rx_burst 轮询 | 用户态轮询 UMEM RX ring |
| **发包方式** | dev_queue_xmit → qdisc → 驱动 | rte_eth_tx_burst 直接写 MMIO | 填入 TX ring，内核 COMPLETION ring 确认 |
| **设备所有权** | 内核独占 | PMD 独占（devbind 接管） | 内核保留所有权，XDP hook 在驱动层 |
| **观测手段** | eBPF / netstat / ethtool / /proc | 软件 stats + ethdev stats | eBPF + 自维护 stats map |
| **虚拟化** | veth / bridge / macvlan | vhost-user / virtio-user | 不直接涉及（但可在 guest 内使用） |
| **部署复杂度** | 零（内核自带） | 高（hugepage、devbind、UIO/VFIO） | 中（需要 XDP 支持的 NIC + libbpf） |

---

## 3. 包处理流程对比（按实验数据）

### 3.1 Kernel Netdev — 教学驱动验证（stage00-14）

```
用户态发包(send) → ndo_start_xmit → TX ring → [backend 模拟硬件]
  → doorbell → backend worker → 构造 skb → IRQ → NAPI schedule
  → napi_poll → netif_receive_skb → 协议栈 → 用户态 recv
```

**实测数据**（stage08 async 链路）:
- 51 包完成完整链路
- 延迟: submit→doorbell=140ns, doorbell→backend=45524ns, backend→irq=70ns, irq→poll=4839ns
- 总路径延迟 ~50μs（教学驱动，非真实硬件）

**关键限制**: 所有实验在软件 loopback 教学驱动上完成，没有真实 NIC 性能数据。stage13 offload 标记为 FAIL。XDP hook 已建立（stage14）但只验证了 XDP_DROP（89 包）。

### 3.2 DPDK — 用户态 fastpath（media-gateway-lite + vmxnet3）

```
pcap PMD (无限重放) → rte_eth_rx_burst → mbuf pool
  → classify (IPv4/UDP) → rule match → header rewrite
  → rte_eth_tx_burst → vmxnet3 PMD TX (真网卡)
```

**实测数据**:
- pcap PMD 路径: rx=1.61 亿包，全部 classify/forward/rewrite（6/6 PASS）
- vmxnet3 真网卡 TX: 553 万包，~692K pps，SW stats 与 ethdev stats 完全一致
- cross_trace_demo: DPDK 处理 1.7 亿包，同时 bpftrace 在 kernel 路径观察到 **0 次** netif_receive_skb

**关键限制**: vmxnet3 RX 路径未验证（UIO 无 MSI-X 中断，VMware guest 无 IOMMU，VFIO 不可用）。e1000（VMware 虚拟 82545EM）不兼容 DPDK UIO/VFIO。没有真实物理网卡双口转发数据。

### 3.3 AF_XDP — 原生 fastpath（mini forwarder）

```
veth-peer → veth-xdp → XDP hook → XSKMAP redirect
  → AF_XDP socket → UMEM RX ring → 用户态读取
  → 修改/丢弃/反射 → UMEM TX ring → COMPLETION ring 确认
```

**实测数据**:
- 4 个 Phase 全部 PASS
- UMEM 8MB (4096 frames × 2048 bytes)
- FILL/RX/TX/COMPLETION 四环各 2048 descriptors
- REFLECT mode: rx=3, tx=3, comp=3（首次同时非零，完成四环闭环）
- 模式探测: skb+copy PASS, native+copy PASS, zero-copy UNSUPPORTED (veth 无 DMA)

**关键限制**: 仅做了 3 包级别的 smoke test（ICMP ping），没有 UDP flood 吞吐数据。Zero-copy 不支持（veth 环境，非真实 NIC）。没有真实 NIC（ixgbe/i40e）的性能验证。

---

## 4. 横向对比矩阵

### 4.1 功能维度

| 能力 | Kernel Netdev | DPDK | AF_XDP |
|------|:---:|:---:|:---:|
| 基础收发包 | PASS | PASS | PASS |
| NAPI/轮询模型 | PASS (NAPI) | PASS (polling) | PASS (polling) |
| 多队列 | PASS (2q) | 未测 | 未测 |
| MSI-X | PASS (per-q IRQ) | N/A (polling) | N/A |
| 分类/转发 | 未测 | PASS (1.61 亿包) | PASS (REFLECT 3 包) |
| Header 改写 | 未测 | PASS (1.61 亿包) | 未测 |
| XDP hook | PASS (DROP 89 包) | N/A | PASS (入口必经) |
| zero-copy | N/A | 未测 | 不支持 (veth) |
| offload (checksum/GRO/GSO) | FAIL (stage13) | N/A | N/A |
| page_pool | PASS | N/A | N/A |
| ethtool stats | PASS | PASS | 部分 (ethtool XDP 计数器异常) |
| 多网卡转发 | 未测 | PASS (pcap PMD→vmxnet3 TX) | 未测 |

### 4.2 性能维度

| 指标 | Kernel Netdev | DPDK | AF_XDP |
|------|:---:|:---:|:---:|
| 最大包量验证 | 145 包 (stage09 q0) | **1.7 亿包** (cross_trace) | 6 包 (mini forwarder) |
| 吞吐量 (实测) | 未测 | ~692K pps (vmxnet3 TX) | 未测 |
| 单包延迟 (实测) | ~50μs (教学驱动) | 未测 | 未测 |
| 统计一致性 | N/A | **100%** (SW vs ethdev) | 未测 |

### 4.3 投入成本维度

| 成本 | Kernel Netdev | DPDK | AF_XDP |
|------|:---:|:---:|:---:|
| 环境搭建 | 零 | 高 (hugepage/devbind/PMD) | 中 (libbpf/clang/内核版本) |
| 代码量 | 驱动 ~500 行/阶段 | fastpath-lite ~900 行 C | forwarder ~200 行 C + ~100 行 BPF |
| 调试难度 | 低 (dmesg/debugfs) | 高 (PMD 层不可见) | 中 (bpftool 可查 prog/map) |
| 部署约束 | 无 | 独占网卡 | 需要 XDP 驱动支持 |

---

## 5. 机制互补关系

三条路径不是互相替代，而是**逐层递进**：

```text
Kernel Netdev ──────> 基础模型：skb/NAPI/ring/queue/interrupt
     │                        │
     │                  建立基准坐标后
     │                        │
     ├──> Real Driver ──> 验证模型在真实代码中成立（virtio_net/e1000e）
     │
     ├──> DPDK ────────> 追求性能：绕过内核，PMD 独占，用户态轮询
     │                        │
     │                   优势在于高性能、完全控制
     │                   代价在于部署复杂、观测困难
     │
     └──> AF_XDP ──────> 追求原生性能：保留内核 hook，XDP 加速
                              │
                         优势在于原生路径、eBPF 生态
                         代价在于 zero-copy 依赖 NIC/驱动
```

### 具体来说

1. **Kernel netdev 是理解另外两条路径的前提。** 只有理解了 NAPI poll、ring、DMA、中断模型，才能理解 DPDK 为什么要自己做 polling，AF_XDP 为什么要用四环模型。

2. **DPDK 和 AF_XDP 从两个方向解决同一个问题** — 如何让包不进内核协议栈就完成转发：
   - DPDK: 直接把网卡从内核抢走（PMD 独占）
   - AF_XDP: 在内核入口点（XDP hook）把包 redirect 走

3. **eBPF observability 是横切层，但覆盖范围不同**：
   - DPDK 路径上，内核 kprobe 看不到任何包（cross_trace_demo 已验证: napi_poll=0）
   - AF_XDP 路径上，XDP hook 和 XSKMAP redirect 在内核可见范围内
   - Kernel netdev 路径上，全程可观测

---

## 6. 选型判断框架

基于本项目的实验数据和机制分析，每条路径适用场景如下：

| 场景 | 推荐路径 | 原因 |
|------|---------|------|
| 学习 Linux 网络驱动 | Kernel Netdev | 建立 skb/NAPI/ring/queue 的基础模型 |
| 读懂真实 NIC 驱动代码 | Real Driver | 将教学模型映射到工业代码 |
| 需要最大吞吐、完全控制数据面 | DPDK | PMD 轮询绕过内核协议栈，用户态完全控制 |
| 需要 Linux 原生路径 + 适当加速 | AF_XDP | XDP hook 保留内核路径入口，但数据面在用户态 |
| 需要内核级观测和定位 | eBPF + Kernel | 全程可 trace，drop reason 可定位 |
| 虚拟化/云网络 | Kernel Netdev → Virtual Net → DPDK vhost | tap/bridge 到 vhost-user，逐步卸掉内核 |
| 已有 DPDK 生态的项目 | DPDK | PMD 库成熟，社区支持强 |
| 不想独占网卡、需要内核网络同时可用 | AF_XDP | 内核保留设备所有权 |

---

## 7. 本项目已证明 vs 未证明

### 已证明（有实验数据支撑）

```text
PASS:
  Kernel netdev: skb/NAPI/ring/queue/MSI-X/page_pool/ethtool/XDP 模型（15 阶段）
  DPDK: PMD/EAL/mbuf/rx_burst/tx_burst + UDP classify/forward/rewrite（pcap PMD 1.61 亿包）
  DPDK: vmxnet3 真网卡 TX（553 万包，692K pps，SW/ethdev stats 一致）
  DPDK: kernel bypass 已证实（cross_trace: DPDK 1.7 亿包 vs kernel 0 次 poll）
  AF_XDP: XDP redirect + AF_XDP socket + UMEM + 四环模型 + mini forwarder 闭环
  AF_XDP: copy/native/zero-copy 模式探测（copy 支持，ZC 不支持在 veth）

BLOCKED / 边界:
  vmxnet3 RX: UIO 无 MSI-X 中断 + VMware 无 IOMMU
  e1000: VMware 虚拟 82545EM 不兼容 DPDK UIO/VFIO
  AF_XDP zero-copy: veth 环境不支持（需要真实物理网卡 DMA）
```

### 未证明（需要不同环境或新实验）

```text
  真实物理网卡（非虚拟）的全部路径性能数据
  DPDK vs AF_XDP 同硬件、同流量 profile 的 head-to-head 吞吐对比
  生产级大规模压测（多队列/RSS/多线程调度）
  Kernel forwarding 与 DPDK/AF_XDP 的性能差距量化
  CPU 利用率、cache miss、中断率的横向对比
```

---

## 8. 结论

这个项目的核心价值不是"做了三条路径"，而是证明了：

1. **能从 Linux 内核网络驱动的完整模型出发**（skb/NAPI/ring/queue/MSI-X/page_pool/ethtool/XDP，15 个阶段全部 PASS）
2. **能把模型映射到工业代码**（virtio_net/e1000e source dive + ethtool patch + trace）
3. **能在用户态构建高性能数据面**（DPDK fastpath-lite/media-gateway-lite，1.61 亿包全部 classify/forward/rewrite）
4. **能理解 Linux 原生 fastpath 的入口和边界**（AF_XDP 四环闭环，copy 可用 / zero-copy 不支持）
5. **能用 eBPF 验证路径行为**（cross_trace_demo: DPDK 处理 1.7 亿包时 kernel 路径完全命中为 0）

三条路径的选择本质上是**性能 vs 集成成本 vs 可观测性**的权衡，本项目通过实验数据让这个权衡变得具体、可讨论。
