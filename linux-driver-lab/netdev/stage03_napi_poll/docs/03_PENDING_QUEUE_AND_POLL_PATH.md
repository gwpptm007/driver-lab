# 03_PENDING_QUEUE_AND_POLL_PATH

## 关键数据路径

```text
user sender
  -> ndo_start_xmit()
  -> stage03_build_rx_skb()
  -> skb_queue_tail(pending_rxq)
  -> stage03_raise_irq()
  -> napi_schedule
  -> stage03_napi_poll(budget)
  -> skb_dequeue() x N
  -> netif_receive_skb()
  -> queue empty ? napi_complete_done() : keep polling
```

## `pending_rxq` 的作用

它不是“最终要交付的设计”，而是这一阶段的教学替身：

- 用来代替真实硬件 RX ring
- 用来解释“包先到了，但暂时不立刻处理”
- 用来承载 budget 限制下的剩余工作

## `budget` 在这里怎么理解

`budget` 不是“最多收多少包”的全局限额，
而是：

> **本次 poll 调用最多允许做多少个 work item。**

在 stage03 里，一个 work item 就是“从 pending_rxq 取出一帧并注入协议栈”。

### 如果 queue 没清空但 budget 用完了
- poll 返回 `work_done == budget`
- NAPI 框架会继续安排后续 poll
- 统计里会看到 `budget_exhausted++`

### 如果 queue 已经清空
- 调用 `napi_complete_done()`
- 重新开“中断”
- 等待下一波 RX

## 中断抑制在 stage03 里的对应物

真实驱动里通常是：
- irq 进来后先 mask 硬件中断
- poll 做完再 unmask

stage03 里用 `irq_masked` 这个布尔状态表达同样的语义：
- `stage03_raise_irq()` 第一次 schedule 时把它置 true
- `napi_complete_done()` 成功时再清回 false

这使得你能在 debugfs 里看到：
- irq_raised
- irq_masked_count
- irq_unmasked_count

## 为什么这一步对 stage04 很重要

等到 stage04 引入真实 ring / DMA 后：
- `pending_rxq` 会被真正的 ring 取代
- `skb_dequeue()` 会被“读 descriptor + unmap + refill”取代

但 stage03 里建立的这些概念会原样保留：
- pending work
- budget
- poll drain
- complete / re-enable
