# 07_DEEP_LEARNING — stage07 深度指南

## 一、stage07 在整个 netdev 学习路径中的位置

stage07 是 W5 末期的最后一跳，承接 stage04 的 ring + DMA + refill，在 stage05 的 virtio-net 源码对照和 stage06 的 ARM64 迁移方法论之后，把驱动模型本身从"教学型 owner 模型"推进到"准真实的 index + queue lifecycle 模型"。

```
W5: DMA + performance (day29-35)
├── day29-32: DMA / mmap / benchmarking / perf
├── day33: stage04 ring + DMA + RX replenishment
├── day34: stage05 virtio-net 源码阅读 + 平台参数化
├── day35: stage06 ARM64 迁移                   ← 平台能力
└─────────────────────────────────────────────────────────────
    stage07 real queue model                    ← 驱动模型深化
```

**stage07 的本质**：把 stage04 的"descriptor 在流转"升级成"queue 是状态机"，让 6 个 index（submit/notify/complete/post/device/consume）各司其职，边界清晰。

---

## 二、stage04 vs stage07：核心差异

### 2.1 数据结构对比

stage04 的核心问题：TX 和 RX 边界模糊，owner 模型在教学上容易解释但与真实驱动差距较大。

| 维度 | stage04 | stage07 |
|------|---------|---------|
| TX 状态追踪 | `tx_prod`（生产者指针） | `submit_idx / notify_idx / complete_idx` |
| RX 状态追踪 | `rx_hw_pos / rx_poll_pos` | `post_idx / device_idx / consume_idx` |
| queue 结构 | 单一 ring，TX 复用 RX slot | TX queue + RX queue 完全分离 |
| device 处理 | 在 `ndo_start_xmit` 里直接完成 | 显式 `stage07_kick_device()` 模拟 backend |
| TX/RX 关系 | RX buffer 直接作为 TX copy 目标 | TX memcpy → 独立 RX buffer（更真实） |
| slot 状态 | owner（CPU/DEV） | `FREE / POSTED / SUBMITTED / DONE` 四状态 |

### 2.2 为什么从 owner 模型推进到 index 模型

stage04 的 owner 模型：

```
CPU 持有 slot → 标记 owner=DEV → device 处理 → 标记 owner=CPU
```

优点：概念简单，适合教学。
缺点：owner 切换逻辑与真实驱动（virtio-net 的 avail/used ring + head/tail index）差距较大。

stage07 的 index 模型：

```
TX: submit_idx (CPU提交) → notify_idx (device消费) → complete_idx (CPU回收)
RX: post_idx (CPU补充) → device_idx (device写入) → consume_idx (CPU消费)
```

优点：与 virtio-net 的 avail ring / used ring 思想完全对应，index 推进单向且不可逆，状态机语义清晰。

---

## 三、TX 队列生命周期详解

### 3.1 三段式流程

```
CPU (ndo_start_xmit)                    device (stage07_kick_device)
  |                                              |
  | 1. 分配 TX slot                             |
  | 2. DMA map skb                               |
  | 3. desc[dma_addr, len] = SUBMITTED           |
  | 4. submit_idx++  (CPU → submit)             |
  | 5. tx_inflight++                             |
  |--------------------------------------------->| 6. 取 notify_idx 位置的 desc
  |                                              | 7. memcpy(TX skb → RX buffer)
  |                                              | 8. desc state = DONE
  |                                              | 9. notify_idx++ (device → consumed)
  |                                              | 10. raise irq / napi_schedule()
  |<--------------------------------------------|
  | 11. NAPI poll: 取 complete_idx              |
  | 12. DMA unmap skb                           |
  | 13. dev_consume_skb_any()                   |
  | 14. slot = FREE                             |
  | 15. complete_idx++ (CPU →回收)              |
  | 16. tx_inflight--                            |
```

### 3.2 关键统计指标（来自 smoke 实测）

```
tx_submit_count=126    ← CPU 提交了 126 个 TX 请求
tx_complete_count=126  ← CPU 回收了 126 个 TX slot
tx_inflight=0          ← 全部完成，无 pending
tx_dropped=0           ← 零丢包
device_notify_count=126 ← backend 收到全部 126 个请求
device_tx_processed=126 ← backend 处理了全部 126 个
```

关键结论：`submit_count == complete_count == device_notify_count == device_tx_processed == 126`，四者完全对齐，说明 TX 路径没有泄漏。

### 3.3 TX 路径的边界保护

```c
// 在 stage07_xmit() 中
if (priv->tx_inflight >= priv->txq.size) {
    netif_stop_queue(ndev);  // 队列满，告知上层暂停
    return NETDEV_TX_BUSY;
}
```

实测 `ring_full_count=0`，说明 queue 容量管理正常，128 深度的 ring 未溢出。

---

## 四、RX 队列生命周期详解

### 4.1 四段式流程

```
CPU (init / refill)                    device (stage07_kick_device)              CPU (NAPI poll)
  |                                              |                                        |
  | 1. post_idx 位置的 slot = FREE              |                                        |
  | 2. 分配 skb                                 |                                        |
  | 3. DMA map skb                              |                                        |
  | 4. slot = POSTED, desc = POSTED            |                                        |
  | 5. post_idx++  (CPU → posted)              |                                        |
  | 6. rx_posted++                              |                                        |
  |<--------------------------------------------|                                        |
  |                                              | 7. 取 device_idx 位置的 RX POSTED slot |
  |                                              | 8. memcpy(TX data → RX skb->data)     |
  |                                              | 9. slot = DONE, desc = DONE           |
  |                                              | 10. device_idx++ (device → filled)     |
  |                                              | 11. rx_ready++                         |
  |                                              | 12. raise irq / napi_schedule()       |
  |                                              |<----------------------------------------|
  |                                              |                                        | 13. consume_idx 位置的 DONE slot
  |                                              |                                        | 14. DMA unmap skb
  |                                              |                                        | 15. eth_type_trans + netif_receive_skb
  |                                              |                                        | 16. slot = FREE, consume_idx++
  |                                              |                                        | 17. rx_ready--
  |                                              |                                        | 18. stage07_refill_one() → 重新 post
```

### 4.2 关键统计指标（来自 smoke 实测）

```
rx_post_count=254      ← 共 post 了 254 次（含初始填充 + 后续 refill）
rx_consume_count=126  ← NAPI 消费了 126 个完成包
rx_refill_count=254   ← refill 254 次
rx_packets=126        ← 实际上送协议栈 126 个包
rx_dropped=0          ← 零丢包
rx_truncated=0        ← 零截断
rx_no_posted=0        ← backend 从未因缺 posted buffer 等待
rx_ready=0            ← NAPI poll 后全部消费完毕
rx_posted=128         ← 当前 128 个 slot 全部已 POSTED（refill 充足）
```

关键结论：
- `rx_post_count=rx_refill_count=254` 说明 refill 次数与 post 一致，没有泄漏
- `rx_no_posted=0` 说明 RX buffer 始终充足，backend 从未停工
- `rx_posted=128`（全部 128 个 slot）在 smoke 结束后全部处于 POSTED 状态

### 4.3 RX consume 与 refill 的边界

stage07 的一个关键设计原则：**consume 和 refill 不在同一个函数里混在一起**。

```c
// stage07_consume_rx_one() 只做一件事：消费一个完成包
static int stage07_consume_rx_one(struct stage07_priv *priv)
{
    // 取 consume_idx 的 DONE slot
    // DMA unmap
    // netif_receive_skb()
    // 触发 refill（调用 stage07_refill_one）
    return 1;
}
```

这样设计的好处：
1. consume 的边界清晰（只取 consume_idx 的已完成包）
2. refill 在 consume 之后触发，保证 slot 立即被补充
3. 两者的统计分开（`rx_consume_count` vs `rx_refill_count`）

---

## 五、NAPI / IRQ / notify 边界设计

### 5.1 三层职责划分

stage07 最重要的设计决策：**IRQ 只负责"叫醒"，不做事**。

```c
// stage07_raise_irq() — irq handler 的全部工作
static void stage07_raise_irq(struct stage07_priv *priv)
{
    bool do_schedule = false;
    spin_lock_irqsave(&priv->state_lock, flags);
    if (!priv->irq_masked) {
        priv->irq_masked = true;  // 防止重复触发
        do_schedule = true;
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);

    if (!do_schedule) return;

    atomic64_inc(&priv->irq_count);
    atomic64_inc(&priv->irq_mask_count);
    atomic64_inc(&priv->napi_schedule_count);
    napi_schedule(&priv->napi);  // 只负责叫醒 NAPI
}
```

**IRQ 做了什么**：计数 + 置 mask 标志 + schedule NAPI。

**IRQ 没做什么**：没有 DMA 操作，没有 memcpy，没有 buffer 回收。

### 5.2 NAPI poll 的批处理

```c
static int stage07_poll(struct napi_struct *napi, int budget)
{
    // 先回收 TX（不限 budget）
    while (stage07_complete_tx_one(priv))
        ;

    // 再消费 RX（按 budget 上限）
    while (work_done < budget && stage07_consume_rx_one(priv))
        work_done++;

    // budget 用尽判断
    if (work_done == budget) {
        budget_exhausted = true;
        atomic64_inc(&priv->napi_budget_exhaust_count);
    }

    // 无更多 RX 时 complete
    if (!more_rx) {
        napi_complete_done(napi, work_done);
        priv->irq_masked = false;
    }
}
```

### 5.3 关键统计（来自 smoke 实测）

```
irq_count=125              ← irq 触发了 125 次
napi_schedule_count=125    ← NAPI schedule 了 125 次
napi_poll_count=125       ← poll 被调用了 125 次
napi_complete_count=125   ← complete 了 125 次
napi_budget_exhaust_count=0 ← budget 从未用尽
napi_work_total=126       ← 总共处理了 126 个包
```

关键观察：
- `irq_count == napi_schedule_count == napi_poll_count == napi_complete_count == 125`，四者完全对齐
- `napi_budget_exhaust_count=0` 说明 32 的 NAPI weight 远大于每次 poll 的实际工作量（最多几个包）
- `napi_work_total=126` 与 `rx_packets=126` 匹配，说明 NAPI 正确处理了所有 RX 包

### 5.4 notify / kick_device 的本质

stage07 的 `stage07_kick_device()` 承担了真实驱动中"device 中断处理"或"doorbell 中断"的工作：

```c
static void stage07_kick_device(struct stage07_priv *priv)
{
    // TX: 把 SUBMITTED slot 标记为 DONE（真实驱动：device DMA 完成）
    // RX: 把 POSTED slot 填入数据并标记为 DONE（真实驱动：device 写入网络帧）
    // 如果产生了 TX DONE 或 RX DONE → raise irq
}
```

在真实 virtio-net 中，这段逻辑发生在：
- virtio-net 的 TX completions 路径（used ring 更新）
- virtio-net 的 RX 填充路径（avail ring 消费 + used ring 写入）

---

## 六、与 virtio-net 的结构映射

### 6.1 TX 路径映射

| stage07 概念 | virtio-net 对应 | 说明 |
|-------------|----------------|------|
| `submit_idx` | avail ring `idx`（的位置） | CPU 提交descriptor 到 avail ring |
| `notify_idx` | device 消费 avail ring | device 读取 avail->ring[head] |
| `complete_idx` | used ring `idx`（的位置） | device 写回 used ring |
| `stage07_kick_device()` | `virtnet_tx()::vp_hdrlen + xmit` | 通知 backend 处理 |
| DMA unmap + free skb | reclaim used buffer | CPU 回收已完成 buffer |

### 6.2 RX 路径映射

| stage07 概念 | virtio-net 对应 | 说明 |
|-------------|----------------|------|
| `post_idx` | avail ring 预先放入 buffer | 提前 post RX buffer |
| `device_idx` | device 写入 RX buffer | device DMA 填充数据 |
| `consume_idx` | used ring 读出已填充 buffer | CPU 取回完成的 RX |
| `stage07_refill_one()` | 重新补充 avail ring | 保证 RX slot 不空 |

### 6.3 为什么这个映射重要

不是"为了对应 virtio-net 而对应"，而是：

> **当你能在自己的驱动里清楚解释 TX submit→notify→complete 和 RX post→device→consume 时，你就能理解 virtio-net 在做什么——而不只是"看代码"。**

stage07 把 virtio-net 的核心队列操作"翻译"成了你自己的模型：

```
virtio-net: avail ring / used ring / vring_desc / vring_used_elem
stage07:    submit_idx   / complete_idx / desc[].state / device_*

本质上都是：producer 写 index → consumer 读 index → 状态机推进
```

---

## 七、教学模型 vs 真实驱动的差距

stage07 仍然是一个**教学型伪设备**，与真实驱动的差距：

| 维度 | stage07 | 真实驱动（如 virtio-net） |
|------|---------|--------------------------|
| device 处理 | `memcpy()` 同步模拟 | 真实 DMA（或 vhost-net backend） |
| 中断触发 | `napi_schedule()` 软件模拟 | 真实 MSI-X 中断 |
| buffer 管理 | 预分配固定数量 | 动态、大小可变、mergeable buffer |
| 多队列 | 单队列 | 多队列 + RSS |
| offload | 无 GRO/GSO/TSO | 完整 offload 栈 |
| 通知机制 | 函数调用 | PCI doorbell 或 virtqueue kick |

**这些差距是刻意保留的**：stage07 的目标是讲清楚 queue lifecycle，不是做成真实网卡。

---

## 八、smoke 测试全流程与观测点

### 8.1 实测数据（2026-04-15，192.168.65.135）

```
tx_submit_count=126       tx_complete_count=126     (差值=0)
rx_post_count=254         rx_consume_count=126     (post>consume，因为 refill)
rx_packets=126            rx_dropped=0
tx_dropped=0              rx_truncated=0           (零异常)
irq_count=125             napi_poll_count=125      (完全对齐)
napi_budget_exhaust_count=0                        (budget 充足)
ring_full_count=0         ring_empty_count=0       (队列状态正常)
rx_no_posted=0                                    (backend 从不缺 buffer)
```

### 8.2 验证方法

所有 index 收敛到稳定状态的验证：

```bash
# TX queues 最终状态：全部 FREE
TX submit=127 notify=127 complete=127 inflight=0 done=0
  tx[*] desc=FREE slot=FREE

# RX queues 最终状态：全部 POSTED（已 refill）
RX post=127 device=127 consume=127 posted=128 ready=0
  rx[*] desc=POSTED slot=POSTED
```

---

## 九、关键设计决策与原理

### 决策 1：为什么 TX complete 不在 IRQ 里做

IRQ 是异步中断上下文，存在竞争条件。如果在 IRQ 里做复杂的 DMA unmap + skb free + queue wake：
- 可能触发锁竞争
- 中断处理时间不可控（影响系统延迟）
- 与其他中断的处理顺序不确定

正确做法：**IRQ 只负责"叫醒"，NAPI poll 负责"做事"**。这正是真实网络驱动的设计。

### 决策 2：为什么 `stage07_kick_device()` 是同步函数

在 stage07 的模拟中，`stage07_kick_device()` 在 `ndo_start_xmit()` 里被**同步调用**：

```c
static netdev_tx_t stage07_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    // ... 提交 slot ...
    stage07_kick_device(priv);  // 同步通知 backend
    return NETDEV_TX_OK;
}
```

在真实 virtio-net 中，这个 kick 是异步的（通过 PCI doorbell 通知 vhost-net），但 stage07 用同步 memcpy 模拟，这样可以在单 CPU 路径上完整演示"提交→处理→完成"的流程。

### 决策 3：为什么 `napi_complete_done()` 后要 re-enable irq

```c
if (!more_rx) {
    napi_complete_done(napi, work_done);  // 关闭 NAPI
    priv->irq_masked = false;              // 允许下次 irq 触发
}
```

NAPI 是"轮询模式"，进入后禁用原有中断，退出后恢复。如果 irq_masked 不重置，下一个 packet 到来时 IRQ 无法再次 schedule NAPI，系统就死了。

### 决策 4：为什么 `rx_post_count=254` 但 `rx_consume_count=126`

初始时 `post_idx` 从 0 推进到 127（128 个 slot 全满），之后每 consume 一个就 refill 一个，所以 refill 次数等于 post 次数。254 = 初始 post(128) + 后续 refill(126)。

---

## 十、与 stage04 的完整对比总结

| 维度 | stage04 | stage07 |
|------|---------|---------|
| **队列模型** | owner-based（tx_prod/rx_hw_pos） | index-based（6个显式 index） |
| **TX/RX 边界** | 混合，RX slot 作为 TX copy 目标 | 完全分离的 txq / rxq |
| **device 角色** | 在 xmit 里直接 memcpy | `stage07_kick_device()` 显式模拟 backend |
| **状态机** | owner=CPU/DEV 两值切换 | FREE/POSTED/SUBMITTED/DONE 四状态 |
| **notify 机制** | 无显式 notify | `stage07_kick_device()` = "通知 backend" |
| **NAPI 设计** | poll 较完整 | poll 职责更明确（TX complete + RX consume 分开） |
| **教学 vs 真实** | 偏向教学 | 更接近 virtio-net 的队列语义 |
| **debugfs** | stats + rings | stats + queues（index 可直接观测） |
| **代码行数** | ~1328 行 | ~932 行（更精简，因为 helper 抽象更好） |

---

## 十一、扩展方向（stage08+）

1. **多队列**：从单队列扩展到多 TX queue + 多 RX queue
2. **真实 backend**：用 vhost-net 或真正的 PCI 设备替代 memcpy 模拟
3. **offload**：添加 GRO/GSO/TSO 支持
4. **XDP**：在 RX 路径上接入 XDP hook
5. **性能基准**：用 iperf / pktgen 测吞吐延迟，与 stage04 对比

---

## 十二、一句话总结

> **stage07 的核心收获是：理解了"queue 是状态机，index 是状态机的推动力"——而不是只知道"descriptor 在流转"。当你能在 smoke 测试里看到 `submit_idx=notify_idx=complete_idx=127` 的收敛，就证明 TX 生命周期没有泄漏；当你看到 `rx_post_count=254` 且 `rx_no_posted=0`，就证明 RX buffer 管理没有空洞。**
