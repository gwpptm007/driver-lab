# STAGE_OVERVIEW

## stage03 真实目标

stage03 不是"把 stage02 换成 napi API 版本"。

它真正要建立的是这条理解链：

```
TX 到达
  -> 构造 RX skb
  -> 先进入 pending queue
  -> 触发一次教学型 irq
  -> irq 只负责 schedule NAPI
  -> poll 按 budget 批量 drain queue
  -> queue 空时 complete + 重新开 irq
```

关键词：**pending queue / irq-poll边界 / budget / complete / 中断抑制**

---

## 与 stage02 的边界对比

### stage02 做了什么
- `ndo_start_xmit()` 里直接构造 RX skb
- 直接 `netif_rx()` 送回协议栈
- 重点是 `skb` 生命周期

### stage03 多做了什么
- 不再立刻处理 RX，先把 RX skb 放进 pending queue
- 再由 `napi_poll()` 批量取出
- 在 poll 上下文里调用 `netif_receive_skb()`
- 显式记录 irq/schedule/poll/budget/complete 统计

---

## 为什么需要 NAPI

> **把"每包一中断"变成"中断只负责通知，真正处理交给 poll 批量完成"。**

高频流量下：
- 每秒 1Gbps → 每秒约 150 万帧
- 每帧一个 IRQ → CPU 被 IRQ 打爆
- 大量 CPU 时间花在 IRQ 处理上，而不是协议栈处理

NAPI 的核心效果：
- IRQ 次数大幅减少（从"每包一IRQ"变成"每波包一IRQ"）
- 批量处理减少 per-packet 开销

---

## stage03 的教学型抽象

stage03 没有真实硬件，所以把这件事抽象成：

- `pending_rxq` 代替硬件 RX ring
- `stage03_raise_irq()` 代替硬件 irq
- `napi_schedule_prep() / __napi_schedule()` 代替真实 irq handler 中的 schedule
- `stage03_napi_poll()` 代替 poll handler

---

## pending_rxq 的作用

它不是"最终要交付的设计"，而是这一阶段的教学替身：

- 用来代替真实硬件 RX ring
- 用来解释"包先到了，但暂时不立刻处理"
- 用来承载 budget 限制下的剩余工作

---

## budget 怎么理解

> **本次 poll 调用最多允许做多少个 work item。**

- 如果 queue 没清空但 budget 用完了 → poll 返回 `work_done == budget`，NAPI 框架继续安排后续 poll
- 如果 queue 已经清空 → 调用 `napi_complete_done()`，重新开"中断"

---

## 中断抑制的语义

真实驱动里通常是：
- irq 进来后先 mask 硬件中断
- poll 做完再 unmask

stage03 里用 `irq_masked` 这个布尔状态表达同样的语义：
- `stage03_raise_irq()` 第一次 schedule 时把它置 true
- `napi_complete_done()` 成功时再清回 false

---

## netif_rx vs netif_receive_skb

| 模式 | 注入 API | 说明 |
|------|----------|------|
| direct | `netif_rx()` | 沿用 stage02 路径，便于对照 |
| napi | `netif_receive_skb()` | 在 poll 上下文注入 |

- `netif_rx()` 触发 NET_RX_SOFTIRQ 延迟处理（非 NAPI）
- `netif_receive_skb()` 在 poll 上下文直接处理（NAPI）

---

## direct vs napi 模式对比

### direct 模式
```
start_xmit -> build_rx_skb -> netif_rx
```
特点：路径短、无排队、无 budget 语义、便于理解 skb

### napi 模式
```
start_xmit -> build_rx_skb -> enqueue pending_rxq -> raise irq -> napi poll -> netif_receive_skb
```
特点：完整的 NAPI 语义、有排队与批处理、有 budget 限制

---

## 与 stage04 的关系

stage03 明确不做：真实 ring descriptor / DMA / RX buffer refill / 多队列

等到 stage04 引入真实 ring / DMA 后：
- `pending_rxq` 会被真正的 ring 取代
- `skb_dequeue()` 会被"读 descriptor + unmap + refill"取代

但 stage03 里建立的这些概念会原样保留：pending work / budget / poll drain / complete / re-enable
