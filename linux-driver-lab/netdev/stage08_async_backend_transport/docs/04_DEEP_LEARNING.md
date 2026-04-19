# 04_DEEP_LEARNING — stage08 深度指南

## 一、stage08 在整个 netdev 学习路径中的位置

stage08 是 stage07 的自然延伸，承接 stage07 的"队列模型正确"，推进到"设备边界更真实"。

```
netdev 学习路径
├── stage00-02: netdev 骨架 / skb 路径 / NAPI 基础
├── stage03-04: ring + DMA / 真实设备模型
├── stage05-06: virtio-net 源码 / ARM64 迁移
├── stage07: real queue model（6 个显式 index）  ← 前端边界清晰
└── stage08: async backend transport            ← 设备边界清晰
    └── 下一跳：多队列 / 真实 vhost backend
```

**stage08 的本质**：把 stage07 的 backend 从"同步函数调用"推进成"独立执行体 + 异步完成模型"，让 timeline 可观测成为可能。

---

## 二、stage07 vs stage08：核心差异

### 2.1 架构对比

| 维度 | stage07 | stage08 |
|------|---------|---------|
| backend 角色 | `stage07_kick_device()` 在 xmit 里同步调用 | `backend_workfn` 通过 workqueue 异步执行 |
| doorbell 语义 | 无（同步调用代替） | `doorbell_pending` + `queue_work()` 真正模拟 |
| 完成时机 | xmit 返回前全部完成 | TX submit 后立即返回，实际处理在 backend worker |
| 上下文边界 | 全部在 softirq/NAPI 上下文 | front-end（用户上下文）↔ backend（workqueue）↔ NAPI（softirq） |
| timeline 观测 | 无 | 8 个时间戳全链路可追踪 |

### 2.2 为什么 stage07 的"假异步"不够

stage07 的 `stage07_kick_device()` 是在 `ndo_start_xmit()` 里同步调用的：

```c
// stage07
static netdev_tx_t stage07_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    // ... submit slot ...
    stage07_kick_device(priv);  // ← 同步调用，xmit 还没返回就处理完了
    return NETDEV_TX_OK;
}
```

这在教学上容易理解，但在真实设备里：
- 设备处理是异步的（PCI doorbell → vhost worker → DMA → irq）
- 网络帧的"提交"和"完成"不在同一个调用栈里

stage08 用 workqueue 真实模拟了这个异步边界。

---

## 三、TX 队列生命周期（异步版）

### 3.1 完整异步流程

```
CPU (ndo_start_xmit)          backend_workfn              NAPI poll
  |                                |                          |
  | 1. DMA map skb                 |                          |
  | 2. slot = SUBMITTED            |                          |
  | 3. submit_idx++                |                          |
  | 4. tx_inflight++                |                          |
  | 5. mark_doorbell()              |                          |
  |    → doorbell_pending=true      |                          |
  |    → queue_work(backend_wq)     |                          |
  |    (xmit 立即返回 NETDEV_TX_OK) |                          |
  |                                |                          |
  |                          6. backend 被调度（workqueue）   |
  |                          7. 处理 TX notify_idx 位置 slot |
  |                          8. memcpy + DMA sync             |
  |                          9. slot = DONE                  |
  |                          10. notify_idx++                |
  |                          11. tx_done++                   |
  |                          12. raise irq()                 |
  |                                |                          |
  |                                |                  13. NAPI poll 醒来
  |                                |                  14. complete_tx_one()
  |                                |                  15. DMA unmap + free skb
  |                                |                  16. slot = FREE
  |                                |                  17. complete_idx++
```

### 3.2 关键统计指标

```
tx_submit_count=126       ← CPU 提交了 126 个 TX
tx_complete_count=126    ← NAPI 回收了 126 个 TX
doorbell_count=126       ← doorbell 被写了 126 次
backend_schedule_count=126 ← backend 入队 126 次
backend_run_count=126    ← backend 实际执行 126 次
backend_tx_processed=126 ← backend 处理了 126 个 TX
```

关键验证：`submit_count == doorbell_count == backend_schedule_count == backend_run_count == backend_tx_processed == tx_complete_count`，全链路无泄漏。

---

## 四、RX 队列生命周期（异步版）

### 4.1 完整异步流程

```
CPU (init/refill)              backend_workfn              NAPI poll
  |                                |                          |
  | 1. post_idx 位置分配 skb       |                          |
  | 2. DMA map                      |                          |
  | 3. slot = POSTED               |                          |
  | 4. post_idx++                   |                          |
  | 5. rx_posted++                  |                          |
  |                                |                          |
  |                          6. backend 处理 TX 时            |
  |                             同时生产 RX                   |
  |                          7. memcpy(TX data → RX skb)     |
  |                          8. slot = DONE                  |
  |                          9. device_idx++                |
  |                          10. rx_ready++                 |
  |                          11. raise irq()                 |
  |                                |                          |
  |                                |                  12. NAPI poll 醒来
  |                                |                  13. consume_rx_one()
  |                                |                  14. DMA unmap + netif_receive_skb
  |                                |                  15. slot = FREE
  |                                |                  16. consume_idx++
  |                                |                  17. rx_ready--
  |                                |                  18. refill_one() → 重新 post
```

### 4.2 关键统计指标

```
rx_post_count=254      ← 共 post 了 254 次（初始 128 + refill 126）
rx_consume_count=126  ← NAPI 消费了 126 个完成包
rx_refill_count=254   ← refill 254 次
rx_packets=126        ←实际上送协议栈 126 个
backend_rx_produced=126 ← backend 生产了 126 个 RX
rx_no_posted=0        ← backend 从未因缺 posted buffer 等待
```

---

## 五、调用链（新增核心内容）

### 5.1 TX 路径调用链

```
协议栈
  ↓ (dev_queue_xmit)
ndo_start_xmit()
  ├── dma_map_single()          TX skb DMA 映射
  ├── 检查 tx_inflight >= ring_size → NETDEV_TX_BUSY
  ├── 分配 slot[idx]
  │   └── slot.state = SUBMITTED
  ├── submit_idx++
  ├── tx_inflight++
  ├── timeline.last_submit_ns
  └── stage08_mark_doorbell()
        ├── doorbell_pending = true
        ├── timeline.last_doorbell_ns
        ├── doorbell_count++
        ├── backend_schedule_count++
        └── queue_work(backend_wq, backend_work)
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
              └── 如果有产出: stage08_raise_irq()
                    ├── irq_masked = true
                    ├── timeline.last_irq_ns
                    ├── irq_count++
                    ├── napi_schedule_count++
                    └── napi_schedule(&napi)
                          ↓ (softirq)
                    NAPI poll()
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

### 5.2 RX 路径调用链

```
backend_workfn()               NAPI poll()
  │                              │
  │ 生产和消费都在              消费 (consume_rx_one)
  │ backend_workfn 的 while 循环   │
  │ 中一起处理                    │
  │                              ├── 取 consume_idx 的 DONE slot
  │                              ├── dma_unmap_single()
  │                              ├── skb_put()
  │                              ├── eth_type_trans()
  │                              ├── netif_receive_skb()
  │                              ├── rx_consume_count++
  │                              ├── rx_packets++
  │                              ├── consume_idx++
  │                              ├── rx_ready--
  │                              ├── timeline.last_consume_ns
  │                              └── stage08_refill_one()
  │                                    ├── 检查 post_idx slot 是否 FREE
  │                                    ├── 分配新 skb + DMA map
  │                                    ├── slot = POSTED
  │                                    ├── post_idx++
  │                                    └── rx_post_count++, rx_refill_count++
  │                              │
  │ 关键：TX 处理和 RX 生产在    关键：consume 和 refill
  │ 同一个 while 循环里          在同一个函数里完成
```

### 5.3 关键函数调用关系图

```
                    ┌─────────────────────────────────────┐
                    │         用户空间                    │
                    │    send_stage08_frame              │
                    └──────────────┬──────────────────────┘
                                   │ write()
                    ┌──────────────▼──────────────────────┐
                    │       ndo_start_xmit()              │
                    │  (front-end, 用户上下文)             │
                    └──┬──────────┬──────────────┬─────────┘
                       │          │              │
                  DMA map    submit slot    doorbell
                       │          │              │
                       ▼          ▼              ▼
              ┌────────────────────────────────────────┐
              │          stage08_mark_doorbell()        │
              │  doorbell_pending=true + queue_work()    │
              └──────────────────┬─────────────────────┘
                                 │
                                 ▼ (workqueue 调度延迟)
              ┌────────────────────────────────────────┐
              │        backend_workfn()                 │
              │  (back-end, 专用 workqueue)            │
              │  • 处理 TX (notify_idx → DONE)         │
              │  • 生产 RX (DONE slot)                 │
              │  • raise_irq()                        │
              └──────────────────┬─────────────────────┘
                                 │ irq + napi_schedule
                                 ▼
              ┌────────────────────────────────────────┐
              │           NAPI poll()                    │
              │  (softirq 上下文)                        │
              │  • complete_tx() — TX 回收              │
              │  • consume_rx() — RX 上送协议栈         │
              │  • refill_one() — RX buffer 补充       │
              └────────────────────────────────────────┘
```

---

## 六、doorbell_pending 与 backend_running 的本质

### 6.1 为什么需要 doorbell_pending

workqueue 的 `queue_work()` 是"通知"机制，不是"等待"机制。如果每次 xmit 都无脑 `queue_work()`：

- backend 还没处理完，新的 work 又入队 → 重复处理
- backend 处理完了，新入队的 work 被遗漏 → 数据卡住

`doorbell_pending` 的语义：**"有事要处理，但还没处理完"**。

```c
static void stage08_mark_doorbell(struct stage08_priv *priv)
{
    spin_lock_irqsave(&priv->state_lock, flags);
    priv->doorbell_pending = true;  // 标记"有事"
    spin_unlock_irqrestore(&priv->state_lock, flags);

    queue_work(priv->backend_wq, &priv->backend_work);  // 通知处理
}
```

### 6.2 为什么需要 backend_running

防止 backend 在上一个 work 还没执行完时，被重复入队：

```c
// backend_workfn 开始时
spin_lock_irqsave(&priv->state_lock, flags);
priv->backend_running = true;  // 标记"正在处理"
spin_unlock_irqrestore(&priv->state_lock, flags);

// backend_workfn 结束时
spin_lock_irqsave(&priv->state_lock, flags);
priv->backend_running = false;
if (priv->doorbell_pending && ...) {
    queue_work(...);  // 还有事，再通知一次
}
spin_unlock_irqrestore(&priv->state_lock, flags);
```

### 6.3 doorbell_pending 的重入场景

当 backend 处理了一批 TX/RX 后，发现还有更多待处理时：

```c
// backend_workfn 结束时
if (priv->txq.notify_idx != priv->txq.submit_idx && priv->rx_posted > 0) {
    priv->doorbell_pending = true;  // 还有没处理完的
    atomic64_inc(&priv->backend_requeue_count);
}
// → 下次 xmit 或 NAPI complete 时可能再次 queue_work
```

---

## 七、与 virtio-net 的结构映射

### 7.1 异步边界映射

| stage08 概念 | virtio-net 对应 | 说明 |
|-------------|----------------|------|
| `stage08_mark_doorbell()` | 写 PCI doorbell BAR | 通知设备有 TX 请求 |
| `queue_work(backend_wq)` | vhost-net 轮询 | 设备侧开始处理 |
| `backend_workfn` | vhost-worker 循环 | 设备执行体 |
| `stage08_raise_irq()` | MSI-X 中断 | 通知 driver 有完成 |
| `NAPI poll` | 驱动 IRQ handler | 收割完成队列 |

### 7.2 关键差异（stage08 vs 真实 virtio-net）

| 维度 | stage08 | 真实 virtio-net |
|------|---------|-----------------|
| TX 处理 | memcpy（同步） | 真实 DMA（异步） |
| 延迟模拟 | `backend_delay_us` 可调 | 真实硬件延迟 |
| 并发 | 单 workqueue（串行） | 多 vhost worker（并行） |
| 通知 | queue_work | PCI config write + MSI-X |

---

## 八、教学模型 vs 真实驱动的差距

| 维度 | stage08 | 真实驱动 |
|------|---------|----------|
| backend 执行 | 单 workqueue（串行） | 多 worker threads（并行） |
| 延迟 | `udelay()` 模拟 | 真实 DMA 延迟不可控 |
| TX 处理 | `memcpy()` | DMA scatter-gather |
| 通知机制 | `queue_work()` | PCI doorbell + MSI-X |
| buffer 管理 | 固定 128 深 | 动态、可变大小 |
| 多队列 | 无 | 多队列 + RSS |

---

## 九、关键设计决策

### 决策 1：为什么用 workqueue 而不是 thread

workqueue 有以下优点：
- **内存管理简单**：内核帮你维护 thread pool
- **调度灵活**：可以 delay、requeue、绑定 CPU
- **调试方便**：`ps aux | grep stage08_backend` 即可看到
- **易加延迟**：`udelay(backend_delay_us)` 可模拟真实设备延迟

### 决策 2：为什么 backend 和 NAPI 分离

如果 backend 和 NAPI 在同一个上下文里处理，会有：
- 锁竞争（NAPI 持锁时 backend 无法工作）
- 中断处理时间不可控
- 无法观测前后端边界

分离后：`backend_workfn` 处理数据通路，`NAPI poll` 处理完成回收，边界清晰。

### 决策 3：为什么 timeline 这么重要

stage08 的核心价值是"把异步性变成可观测的"：

```bash
cat /sys/kernel/debug/netdev_stage08/timeline
# delta_submit_to_doorbell_ns       ← xmit 到 doorbell 的延迟
# delta_doorbell_to_backend_ns    ← doorbell 到 backend 执行的延迟
# delta_backend_to_irq_ns          ← backend 处理到 irq 的延迟
# delta_irq_to_poll_ns             ← irq 到 NAPI poll 的延迟
```

当 `backend_delay_us=0` 时，这些 delta 都在微秒级；设为 `100` 时，可以看到清晰的异步阶梯。

---

## 十、扩展方向（stage09+）

1. **多队列**：从单队列扩展到多 TX queue + 多 RX queue + 多 NAPI
2. **真实 backend**：用 vhost-net backend 替代 workqueue
3. **batch 优化**：backend_batch 参数的性能影响
4. **延迟 profile**：不同 backend_delay_us 下的 timeline 分布
5. **性能基准**：与 stage07 对比吞吐/延迟

---

## 十一、nds8 是怎么被创建出来的

### 11.1 从 ifconfig 看到的特征解释

```bash
nds8: flags=4291<UP,BROADCAST,RUNNING,NOARP,MULTICAST>  mtu 1500
        ether 76:7d:02:43:ff:13  txqueuelen 1000  (Ethernet)
        RX packets 1309  dropped 1053
        TX packets 1309
```

| 字段 | 值 | 说明 |
|------|-----|------|
| `flags=4291` | 含 `NOARP` | 无 ARP 协议，不是真实物理网卡 |
| `ether` | 76:7d:02:43:ff:13 | 驱动硬编码的固定 MAC，不是真实厂商号 |
| `inet` | **无** | 没有 IP 地址，因为设备根本不需要 TCP/IP 栈 |
| `RX packets = TX packets` | 1309 | 所有 TX 帧都通过驱动内部环回到 RX |

**为什么没有 IPv4 地址？** 因为测试工具（`send_stage08_frame` / `recv_stage08_frame`）直接用 **AF_PACKET SOCK_RAW** 发送原始以太网帧，走链路层，根本不需要 IP。

### 11.2 创建流程：4 步从零到 ifconfig 可见

**第 1 步：insmod → `stage08_init()` 调用 `alloc_netdev()`**

```c
// netdev_stage08.c:1819
stage08_dev = alloc_netdev(
    sizeof(struct stage08_priv),  // priv 数据区大小
    ifname,                       // "nds8"
    NET_NAME_UNKNOWN,             // 命名空间：虚拟设备用 UNKNOWN
    stage08_setup                 // 初始化回调
);
```

**第 2 步：`stage08_setup()` — 设置以太网属性**

```c
static void stage08_setup(struct net_device *ndev)
{
    ether_setup(ndev);                // Linux 提供：ETH_HLEN=14, addr_len=6, type=ETHER
    ndev->netdev_ops = &stage08_netdev_ops;
    ndev->flags |= IFF_NOARP;      // ★ 不需要 ARP，Linux 不会分配 IP
    ndev->features |= NETIF_F_HIGHDMA;
    eth_hw_addr_random(ndev);        // ★ 随机生成本地 MAC，不是真实厂商号
}
```

关键标志位：
- `IFF_NOARP` → Linux 知道这个设备不需要 ARP/IP，直接阻断 IP 层配置
- `eth_hw_addr_random()` → MAC 是驱动随机生成的（76:7d:02:43:ff:13），不是真实网卡的

**第 3 步：初始化驱动私有数据 + NAPI + workqueue**

```c
priv = netdev_priv(stage08_dev);              // 从 net_device 拿到 priv 指针
stage08_prepare_dma_caps(ndev);               // DMA 能力（虚拟设备保留此接口）
STAGE08_NETIF_NAPI_ADD(ndev, &priv->napi, stage08_poll, napi_weight);
priv->backend_wq = alloc_ordered_workqueue("stage08_backend", WQ_MEM_RECLAIM);
INIT_WORK(&priv->backend_work, stage08_backend_workfn);
stage08_alloc_queues(priv);                   // 分配 TX/RX ring + 预填充 RX buffers
```

**第 4 步：`register_netdev()` → nds8 正式出现**

```c
register_netdev(stage08_dev);  // ★ 把 netdev 加入内核网络设备链表
                                 // 之后 ip link / ifconfig 就能看到 nds8 了
```

### 11.3 完整流程图

```
insmod netdev_stage08.ko
    ↓
stage08_init()
    ├── alloc_netdev(sizeof_priv, "nds8", stage08_setup)
    │       ↓
    │   stage08_setup()
    │       ├── ether_setup()           // ETH_HLEN=14, addr_len=6, type=ETHER
    │       ├── IFF_NOARP 标志         // ★ 无 ARP，不需要 IP
    │       └── eth_hw_addr_random()   // ★ 随机 MAC，不是真实厂商号
    │
    ├── netif_napi_add()              // 注册 NAPI poll 函数
    ├── alloc_ordered_workqueue()      // 创建 backend workqueue
    ├── stage08_alloc_queues()         // 分配 TX/RX ring，预填充 RX buffers
    │
    └── register_netdev()              // ★ nds8 正式加入内核网络栈
           ↓
ifconfig / ip link show  →  nds8 出现！
```

### 11.4 为什么 RX dropped = 1053

这是驱动内部统计，不是真正"丢包"：

```
RX packets 1309  dropped 1053
TX packets 1309
```

- TX packets = 1309：驱动发出 1309 帧
- RX packets = 1309：驱动内部环回收到 1309 帧
- RX dropped = 1053：驱动在某些测试场景下把 1053 帧标记为 `rx_dropped`（可能是 RX buffer slot 不足、或测试帧格式不对时的计数）

数值相等说明**环回链路是通的**，dropped 是驱动的自我计数，不等同于物理网口的丢包。

---

## 十二、一句话总结

> **stage08 的核心收获是：理解了"前后端边界"和"异步完成模型"——提交（submit）和完成（complete）不在同一个调用上下文里，doorbell_pending 是它们的握手信号，workqueue 是异步处理的载体，timeline 让这个异步链路变得可观测。**
