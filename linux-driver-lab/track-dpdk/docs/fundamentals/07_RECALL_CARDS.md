# DPDK 复习卡与自测

## 1. 一页总图

```text
NIC packet
 -> RX queue / descriptor
 -> DMA into mempool-backed data buffer
 -> PMD poll
 -> rte_eth_rx_burst returns mbuf ownership
 -> parse / classify / rewrite
 -> rte_eth_tx_burst accepts zero or more mbufs
 -> free/retry unsent and dropped mbufs
 -> TX completion reclaims accepted mbufs
```

## 2. 十张核心卡

### 卡 1：EAL

**问：** EAL 为什么必须先初始化？  
**答：** 后续 port、lcore、memory、PCI/vdev 都依赖它建立的运行环境。EAL 参数与应用参数通过 `--` 分层。

### 卡 2：PMD

**问：** PMD 与内核 netdev driver 最大的路径差别？  
**答：** PMD 通常在用户态 lcore 主动轮询 queue，并把 mbuf 直接交给应用；典型内核路径进入 NAPI、skb、协议栈和 socket。

### 卡 3：Hugepage

**问：** 为什么使用 hugepage？  
**答：** 扩大 TLB 覆盖范围，并为大块、稳定的 DMA/IOVA 映射提供基础。它不自动解决 NUMA、cache miss 或对象泄漏。

### 卡 4：Mempool

**问：** 为什么不在每包路径 malloc？  
**答：** mempool 预分配同构对象，配合 per-lcore cache 减少通用 allocator、碎片和共享竞争。

### 卡 5：Mbuf

**问：** mbuf 是 packet 吗？  
**答：** mbuf 主要是 metadata，并指向 backing data buffer；还可能形成多 segment chain。

### 卡 6：Queue

**问：** queue 为什么常与 lcore 一一映射？  
**答：** 明确 ownership，减少共享 queue 的同步、迁核和 cache line 竞争。

### 卡 7：Burst

**问：** burst 越大越好吗？  
**答：** 不是。大 burst 摊薄固定开销，但可能增加排队和尾延迟，需要实测。

### 卡 8：TX ownership

**问：** `tx_burst(pkts, 32)` 返回 20 怎么办？  
**答：** 前 20 个交给 TX，后 12 个仍归应用，必须重试、排队或释放。

### 卡 9：Kernel bypass

**问：** bypass 了什么？  
**答：** 通常绕过 netdev/NAPI/skb/内核协议栈/socket 数据路径；仍依赖内核提供 PCI、VFIO/IOMMU、内存、调度等能力。

### 卡 10：Evidence

**问：** testpmd 启动成功能证明性能吗？  
**答：** 不能。它首先证明 EAL/PMD/port 可初始化；流量与性能需要独立计数、拓扑和测量证据。

## 3. 对比速查

| Kernel path | DPDK path |
|---|---|
| kernel netdev driver | user-space PMD |
| NAPI budget polling | dedicated lcore polling |
| `sk_buff` | `rte_mbuf` |
| socket API | ethdev burst API |
| kernel protocol stack | application-selected processing |
| scheduler/wakeup friendly | core dedication/common busy polling |

| 容易混淆 | 正确区分 |
|---|---|
| descriptor vs mbuf | descriptor 描述 queue/DMA 工作；mbuf 是软件 packet metadata |
| mbuf vs data buffer | mbuf 指向 buffer，二者生命周期相关但概念不同 |
| port vs physical NIC | port 也可以是 vdev |
| vhost socket vs packet path | socket 是控制协商入口；packet 常走共享 virtqueue |
| smoke vs traffic | smoke 证明启动；traffic 证明计数和动作 |

## 4. 五个常见错误表述

1. **“DPDK 完全不经过内核。”** 应改为“目标端口的数据面绕过内核协议栈，设备治理和内存仍依赖内核”。
2. **“hugepage 是为了让整块内存物理连续。”** 应强调 TLB 和 DMA mapping；不要把整个 pool 简化成一个连续物理块。
3. **“rx_burst 返回的就是 packet 数组。”** 返回的是 mbuf pointer 数组。
4. **“tx_burst 调用后全部发送。”** 返回值才表示被接受的前缀数量。
5. **“RX 非零说明项目完成。”** 还要验证 parser/action/TX/drop 守恒和 cleanup。

## 5. 面试式自测

### 基础

1. 从 NIC 到 DPDK 应用描述一次 RX。
2. PMD polling 为什么减少中断和调度成本？
3. mempool 与 mbuf 的关系是什么？
4. data room、headroom、`data_off` 分别是什么？
5. 为什么 EAL 参数后常出现 `--`？

### 正确性

6. 丢包分支忘记 free 会如何逐步表现？
7. 多线程共享 queue 有什么风险？
8. parser 为什么要先检查长度再强转 header？
9. 软件 ring 满时 ownership 应如何处理？
10. 正常退出为什么先停 worker 再销毁 mempool？

### 性能

11. burst 对吞吐和尾延迟的影响是什么？
12. queue、lcore、mempool 为什么要考虑 NUMA？
13. false sharing 如何影响 per-worker stats？
14. polling 的 CPU 成本如何与低延迟收益权衡？
15. pcap/null vdev 结果为什么不能代表真实 NIC？

## 6. 自测答案关键词

```text
1  descriptor -> DMA buffer -> PMD poll -> mbuf ownership
2  no per-packet wakeup, batching, dedicated execution context
3  pool manages reusable mbuf objects
4  buffer capacity / reserved prefix / packet start offset
5  split EAL arguments and application arguments
6  pool drains -> rx_nombuf/drop -> apparent RX stall
7  PMD thread-safety, synchronization, cache contention
8  malformed/truncated/multi-segment packets
9  failed enqueue keeps producer ownership; retry/drop/free by policy
10 prevent use-after-free and in-flight object loss
11 amortization versus queueing/cache working set
12 local memory and PCI locality
13 cache-line bouncing
14 workload SLA and CPU budget
15 software/virtual path lacks real DMA/PCIe/RSS/hardware behavior
```

## 7. 什么时候可以开始项目

如果能不看答案解释以下四件事，就可以进入 `lab-vmxnet3-testpmd` 和 `lab-dpdk-l2-forwarding`：

- DPDK 在内核/NIC 路径中的位置。
- hugepage、mempool、mbuf 的分工。
- RX/TX burst 的所有权变化。
- smoke、traffic 和 performance 三类证据边界。
