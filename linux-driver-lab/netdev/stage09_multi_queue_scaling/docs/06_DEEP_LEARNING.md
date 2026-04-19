# 06_DEEP_LEARNING — stage09 深度指南

## 一、stage09 在整个 netdev 学习路径中的位置

```
netdev 学习路径
├── stage00-02: netdev 骨架 / skb 路径 / NAPI 基础
├── stage03-04: ring + DMA / 真实设备模型
├── stage05-06: virtio-net 源码 / ARM64 迁移
├── stage07: real queue model（6 个显式 index）
├── stage08: async backend transport（单队列异步）← 前后端边界清晰
└── stage09: multi-queue scaling                ← 多队列与分发策略
    └── 下一跳：queue affinity / 真实 vhost backend / 多核并行
```

**stage09 的本质**：把 stage08 的单队列异步扩展为多队列并行，让分发策略、可观测性和并行性成为可能。

---

## 二、stage08 vs stage09：核心差异

### 2.1 架构对比

| 维度 | stage08 | stage09 |
|------|---------|---------|
| 队列数 | 1（全局） | 多（默认 2，最多 4） |
| NAPI | 1 个全局 NAPI | 每队列独立 NAPI |
| backend work | 1 个全局 work item | 每队列独立 work item |
| timeline | 1 个全局 timeline | 每队列独立 timeline |
| stats | 全局计数器 | 每队列独立 atomic64 |
| 队列分发 | 无（单队列不需要） | `ndo_select_queue` 回调 |
| alloc 方式 | `alloc_netdev` + `ether_setup` | `alloc_etherdev_mqs` |

### 2.2 为什么单队列不够

单队列的瓶颈：
- 所有 TX/RX 都在一个 CPU 核上处理，无法利用多核
- 单队列满时，新帧被丢弃（`NETDEV_TX_BUSY`），其他队列空闲
- 无法观测不同队列的负载差异

stage09 的多队列让并行处理、负载均衡和 per-queue 观测成为可能。

---

## 三、per-queue TX 队列生命周期（多队列异步版）

### 3.1 完整异步流程

```
CPU (ndo_start_xmit)           backend_workfn (queue qX)       NAPI poll (queue qX)
  │                                  │                              │
  │  qid = hash % num_queues        │                              │
  │  q = &queues[qid]               │                              │
  │                                  │                              │
  │ 1. DMA map skb                  │                              │
  │ 2. slot = SUBMITTED              │                              │
  │ 3. submit_idx++                  │                              │
  │ 4. tx_inflight++                │                              │
  │ 5. mark_doorbell(q)             │                              │
  │    → q->doorbell_pending=true    │                              │
  │    → queue_work(q->backend_work)│                              │
  │    (xmit 立即返回 NETDEV_TX_OK) │                              │
  │                                  │                              │
  │                            6. backend 被调度（workqueue）        │
  │                            7. 处理 qX 的 TX notify_idx         │
  │                            8. memcpy + DMA sync               │
  │                            9. slot = DONE                      │
  │                            10. notify_idx++                    │
  │                            11. tx_done++                       │
  │                            12. raise_irq(q)                    │
  │                                  │                              │
  │                                  │               13. NAPI poll(q) 醒来
  │                                  │               14. complete_tx_one(q)
  │                                  │               15. DMA unmap + free skb
  │                                  │               16. slot = FREE
  │                                  │               17. complete_idx++
```

### 3.2 关键：每队列独立但共享 workqueue

stage09 每个队列的 `backend_work` 独立，但都入同一个 `backend_wq`：
- `WQ_UNBOUND`：work 不绑定特定 CPU，调度器决定执行位置
- 各队列的 backend 并发入队，互不影响
- 真实 NIC 每队列有独立硬件通道，stage09 用统一 wq + 独立 work item 模拟

---

## 四、per-queue RX 队列生命周期

```
CPU (init/refill)            backend_workfn (queue qX)       NAPI poll (queue qX)
  │                                  │                              │
  │ 1. post_idx 位置分配 skb         │                              │
  │ 2. DMA map                      │                              │
  │ 3. slot = POSTED                │                              │
  │ 4. post_idx++                    │                              │
  │ 5. rx_posted++                   │                              │
  │                                  │                              │
  │                            6. backend 处理 qX 的 TX 时          │
  │                               同时生产 qX 的 RX                   │
  │                            7. memcpy(TX data → RX skb)         │
  │                            8. slot = DONE                      │
  │                            9. device_idx++                    │
  │                            10. rx_ready++                     │
  │                            11. raise_irq(q)                    │
  │                                  │                              │
  │                                  │               12. NAPI poll(q) 醒来
  │                                  │               13. consume_rx_one(q)
  │                                  │               14. DMA unmap + netif_receive_skb
  │                                  │               15. slot = FREE
  │                                  │               16. consume_idx++
  │                                  │               17. rx_ready--
  │                                  │               18. refill_one() → 重新 post
```

---

## 五、调用链

### 5.1 TX 路径调用链（per-queue）

```
协议栈
  ↓ (dev_queue_xmit)
ndo_start_xmit()
  ├── skb_get_queue_mapping(skb) % num_queues  → 选择目标队列 qid
  ├── q = &priv->queues[qid]
  ├── spin_lock(&priv->state_lock)
  ├── skb_linearize()  (if needed)
  ├── dma_map_single()  TX skb DMA 映射
  ├── 检查 tx_inflight >= ring_size → NETDEV_TX_BUSY
  ├── 分配 slot[idx]
  │   └── slot.state = SUBMITTED
  ├── submit_idx++
  ├── tx_inflight++
  ├── timeline.last_submit_ns
  ├── doorbell_count++
  ├── test_tx_submit_count++  (if test frame)
  └── stage09_mark_doorbell(q)
        ├── doorbell_pending = true
        ├── timeline.last_doorbell_ns
        ├── backend_schedule_count++
        └── queue_work(backend_wq, &q->backend_work)
              ↓ (workqueue 调度)
        backend_workfn()
              ├── backend_running = true
              ├── timeline.last_backend_wakeup_ns
              ├── backend_run_count++
              ├── [可选 udelay(backend_delay_us)]
              ├── while (notify_idx != submit_idx && rx_posted > 0)
              │   ├── 处理 TX slot: memcpy + DMA sync
              │   ├── slot.state = DONE
              │   ├── notify_idx++
              │   ├── tx_done++
              │   └── backend_tx_processed++
              ├── timeline.last_backend_done_ns
              ├── backend_running = false
              ├── 如果还有未处理: doorbell_pending = true + requeue
              └── 如果有产出: stage09_raise_irq(q)
                    ├── irq_masked = true
                    ├── timeline.last_irq_ns
                    ├── irq_count++
                    └── napi_schedule(&q->napi)
                          ↓ (softirq)
                    napi_poll()
                          ├── timeline.last_poll_ns
                          ├── napi_poll_count++
                          ├── while (complete_tx_one()) → TX 回收
                          ├── while (budget && consume_rx_one()) → RX 上送
                          ├── napi_work_total += work_done
                          ├── 如果无更多 RX:
                          │   ├── napi_complete_done()
                          │   ├── irq_masked = false
                          │   └── napi_complete_count++
                          └── 返回 work_done
```

### 5.2 队列选择调用链

```
ndo_select_queue()
  ├── skb_get_hash(skb)  → 获取 flow hash
  ├── if (hash != 0)
  │   └── return reciprocal_scale(hash, priv->num_queues)
  │        (hash % num_queues 的优化实现)
  └── else
        └── return atomic64_inc_return(&priv->rr_counter) % priv->num_queues
             (round-robin 兜底)
```

### 5.3 关键函数调用关系图

```
                    ┌─────────────────────────────────────┐
                    │         用户空间                    │
                    │    send_stage09_frame              │
                    └──────────────┬──────────────────────┘
                                   │ write()
                    ┌──────────────▼──────────────────────┐
                    │       ndo_start_xmit()            │
                    │  (front-end, 用户上下文)           │
                    │  qid = select_queue(skb)          │
                    └──┬──────────┬──────────────┬───────┘
                       │          │              │
                  DMA map    submit slot    doorbell
                       │          │              │
                       ▼          ▼              ▼
              ┌────────────────────────────────────────┐
              │       stage09_mark_doorbell(q)         │
              │  doorbell_pending=true + queue_work()  │
              └──────────────────┬─────────────────────┘
                                 │
              ┌──────────────────▼─────────────────────┐
              │     backend_workfn (per-queue qX)      │
              │   (back-end, 共享 backend_wq)         │
              │  • 处理 TX (qX: notify_idx → DONE)    │
              │  • 生产 RX (qX: DONE slot)            │
              │  • raise_irq(qX)                      │
              └──────────────────┬─────────────────────┘
                                 │ irq + napi_schedule
              ┌──────────────────▼─────────────────────┐
              │        NAPI poll (per-queue qX)         │
              │      (softirq 上下文)                  │
              │  • complete_tx() — TX 回收 (qX)        │
              │  • consume_rx() — RX 上送协议栈 (qX)   │
              │  • refill_one() — RX buffer 补充 (qX) │
              └────────────────────────────────────────┘
```

---

## 六、doorbell_pending 与 backend_running（多队列版）

### 6.1 为什么需要 doorbell_pending

workqueue 的 `queue_work()` 是"通知"机制，不是"等待"机制。多队列下这个机制尤为重要：

- 队列 q0 的 backend 还没处理完，q0 又收到新帧 → 不重复入队
- 队列 q1 的 backend 处理完了，q1 又收到帧 → 正常入队

`doorbell_pending` 的语义：**"本队列有事要处理，但还没处理完"**（per-queue 粒度）。

### 6.2 为什么需要 backend_running

防止 backend 在上一个 work 还没执行完时，被重复入队：

```c
// backend_workfn 开始时
q->backend_running = true;

// backend_workfn 结束时
q->backend_running = false;
if (q->doorbell_pending) {
    queue_work(backend_wq, &q->backend_work);  // 还有事，再通知一次
}
```

### 6.3 多队列下的状态隔离

stage09 每个队列的状态完全独立：

```
q0: doorbell_pending=true, backend_running=false  → 有事要处理
q1: doorbell_pending=false, backend_running=true → 正在处理
q2: doorbell_pending=false, backend_running=false → 空闲
```

这与 stage08 全局单一状态完全不同，多队列状态需要各自独立追踪。

---

## 七、queue affinity 与 CPU 绑核

### 为什么要绑核？

1. **缓存友好**：队列的处理总是在同一个 CPU，cache 命中率更高
2. **中断亲和性**：将 IRQ 亲和性设置为特定 CPU，减少跨核中断开销
3. **延迟可预测**：绑核后处理延迟更稳定（没有跨核调度带来的不确定性）

### 内核层面实现

```c
// 将队列的 IRQ 绑定到特定 CPU
irq_set_affinity_hint(irq, cpumask_of(cpu));

// 或者使用 set_affinity() 接口
irq_set_affinity(irq, cpu_mask);
```

### 用户层面

```bash
# 查看 IRQ 亲和性
cat /proc/interrupts | grep eth0
# 设置 IRQ 亲和性
echo 1 > /proc/irq/XXX/smp_affinity  # 绑定到 CPU 0
```

---

## 八、与 virtio-net 的结构映射

| stage09 概念 | virtio-net 对应 | 说明 |
|-------------|----------------|------|
| `stage09_mark_doorbell(q)` | 写 PCI doorbell BAR（per-queue） | 通知设备有 TX 请求 |
| `queue_work(backend_wq, &q->backend_work)` | vhost-net 轮询 | 设备侧开始处理 |
| `backend_workfn`（per-queue） | vhost-worker 循环（per-queue） | 设备执行体 |
| `stage09_raise_irq(q)` | MSI-X 中断（per-queue） | 通知 driver 有完成 |
| `NAPI poll(q)` | 驱动 IRQ handler | 收割完成队列 |

---

## 九、教学模型 vs 真实驱动的差距

| 维度 | stage09 | 真实驱动 |
|------|---------|----------|
| backend 执行 | 共享 workqueue（串行） | 多 worker threads（并行） |
| 多队列 | soft 模拟（4 队列） | 硬件独立通道（16-128 队列） |
| TX 处理 | `memcpy()`（同步） | 真实 DMA scatter-gather |
| 通知机制 | `queue_work()` | PCI doorbell + MSI-X |
| 延迟 | `udelay()` 模拟 | 真实 DMA 延迟不可控 |
| 队列分发 | hash + round-robin | RSS + indirection table |
| 锁粒度 | 全局 `state_lock` | per-queue lock（优化） |

---

## 十、关键设计决策

### 决策 1：为什么用统一 workqueue 而非 per-queue wq

统一 `backend_wq` 的优点：
- **资源控制简单**：一个 wq 管理所有队列的 backend work
- **调度灵活**：WQ_UNBOUND 让调度器决定执行 CPU
- **per-queue 独立**：每个队列的 `backend_work` 独立入队，状态各自追踪

真实硬件：每队列有独立 DMA 引擎和中断通道。stage09 用共享 wq + 独立 work item 模拟。

### 决策 2：为什么 timeline 是 per-queue 的

多队列下，每个队列的异步延迟可能不同：
- 队列 0 和队列 1 的 `doorbell_to_backend_ns` 可能相差很大
- 通过 per-queue timeline 可以观测队列间负载是否均衡
- 如果只有一个队列的 timeline 活跃，说明分发策略有问题

### 决策 3：为什么 hash 优先于 round-robin

- **保序**：同一 5-tuple 的帧到同一队列，帧顺序不变
- **Cache 友好**：同一 CPU 处理同一 flow 的数据
- **真实 NIC RSS**：也是基于 5-tuple hash 的分发

round-robin 作为兜底，保证无 hash 流量也能分散。

---

## 十一、扩展方向（stage10+）

1. **queue affinity / CPU 绑核**：将 NAPI 和 backend_work 绑定到特定 CPU
2. **真实 backend**：用 vhost-net backend 替代 workqueue
3. **TX 调度器**：weighted fair queuing / traffic class 优先级
4. **RSS 扩展**：可配置 hash 类型 + indirection table
5. **零拷贝优化**：`dma_buf` 共享 / page pool 替代每次 alloc_skb
6. **per-queue lock**：从全局 `state_lock` 改为 per-queue lock

---

## 十二、nds9 是怎么被创建出来的

### 12.1 从 ifconfig 看到的特征解释

```bash
nds9: flags=4291<UP,BROADCAST,RUNNING,NOARP,MULTICAST>  mtu 1500
        ether 76:7d:02:43:ff:13  txqueuelen 1000  (Ethernet)
        RX packets 1309  dropped 1053
        TX packets 1309
```

| 字段 | 值 | 说明 |
|------|-----|------|
| `flags=4291` | 含 `NOARP` | 无 ARP 协议，不是真实物理网卡 |
| `ether` | 76:7d:02:43:ff:13 | 驱动硬编码的固定 MAC |
| `inet` | **无** | 没有 IP 地址，设备走链路层 raw socket |
| `RX packets = TX packets` | 1309 | 所有 TX 帧都通过驱动内部环回到 RX |

### 12.2 创建流程

**第 1 步：`insmod → stage09_init() → alloc_etherdev_mqs()`**

```c
ndev = alloc_etherdev_mqs(
    sizeof(struct stage09_priv),  // priv 数据区大小
    num_queues,                   // TX 队列数（默认 2）
    num_queues                    // RX 队列数（默认 2）
);
```

关键：`alloc_etherdev_mqs` vs stage08 的 `alloc_netdev`：
- 自动创建指定数量的 TX/RX 队列
- 内核自动设置 `ndev->num_tx_queues` 和 `ndev->num_rx_queues`

**第 2 步：初始化 priv + backend_wq + 每队列 NAPI + 每队列 ring**

```c
priv = netdev_priv(ndev);
spin_lock_init(&priv->state_lock);
priv->backend_wq = alloc_workqueue("stage09_backend", WQ_UNBOUND | WQ_MEM_RECLAIM, 0);

for (i = 0; i < num_queues; i++) {
    q = &priv->queues[i];
    q->priv = priv;
    q->qid = i;
    INIT_WORK(&q->backend_work, stage09_backend_workfn);
    stage09_alloc_ring(&q->txq, ring_size);
    stage09_alloc_ring(&q->rxq, ring_size);
    STAGE09_NETIF_NAPI_ADD(ndev, &q->napi, stage09_napi_poll, napi_weight);
}
```

**第 3 步：`register_netdev()` → nds9 正式出现**

```c
register_netdev(ndev);  // ★ 把 netdev 加入内核网络设备链表
                         // 之后 ip link / ifconfig 就能看到 nds9 了
```

### 12.3 完整流程图

```
insmod netdev_stage09.ko
    ↓
stage09_init()
    ├── alloc_etherdev_mqs(sizeof_priv, num_queues=2, num_queues=2)
    │       ↓
    │   ether_setup()           // ETH_HLEN=14, addr_len=6, type=ETHER
    │   IFF_NOARP 标志         // ★ 无 ARP，不需要 IP
    │   eth_hw_addr_random()   // ★ 随机 MAC
    │
    ├── spin_lock_init()        // 初始化全局 state_lock
    ├── alloc_workqueue()      // 创建统一 backend_wq（WQ_UNBOUND）
    │
    ├── for each queue (i=0..num_queues-1)
    │   ├── INIT_WORK(backend_work)   // 每队列独立 work item
    │   ├── stage09_alloc_ring(txq)   // 分配 TX ring
    │   ├── stage09_alloc_ring(rxq)   // 分配 RX ring
    │   └── netif_napi_add()          // 每队列注册独立 NAPI
    │
    └── register_netdev()        // ★ nds9 正式加入内核网络栈
           ↓
ifconfig / ip link show  →  nds9 出现！
```

---

## 十三、一句话总结

> **stage09 的核心收获是：理解了"多队列不是数组扩容，而是状态/执行/观测的全面拆分"——每队列独立的 NAPI/backend_work/timeline/stats 让并行处理和负载观测成为可能，hash-based 分发保证了同一 flow 的保序，而 doorbell_pending/backend_running 在 per-queue 粒度上实现了可靠的异步调度。**
