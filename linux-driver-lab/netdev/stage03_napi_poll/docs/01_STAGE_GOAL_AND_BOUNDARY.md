# 01_STAGE_GOAL_AND_BOUNDARY

## stage03 的真实目标

stage03 不是“把 stage02 换成 napi API 版本”。

它真正要建立的是这条理解链：

```text
TX 到达
  -> 构造 RX skb
  -> 先进入 pending queue
  -> 触发一次教学型 irq
  -> irq 只负责 schedule NAPI
  -> poll 按 budget 批量 drain queue
  -> queue 空时 complete + 重新开 irq
```

也就是说，stage03 的关键词不是函数名，而是：

- pending queue
- irq / poll 边界
- budget
- complete
- 中断抑制

## 与 stage02 的边界对比

### stage02 做了什么
- `ndo_start_xmit()` 里直接构造 RX skb
- 直接 `netif_rx()` 送回协议栈
- 重点是 `skb` 生命周期

### stage03 多做了什么
- 不再立刻处理 RX
- 先把 RX skb 放进 pending queue
- 再由 `napi_poll()` 批量取出
- 在 poll 上下文里调用 `netif_receive_skb()`
- 显式记录 irq/schedule/poll/budget/complete 统计

## 这一阶段明确不做什么

- 还不做真实 ring descriptor
- 还不做 DMA / mapping / unmapping
- 还不做 RX buffer refill
- 还不做多队列

理由很简单：

> 如果现在把 ring / DMA 也一起塞进来，你会分不清“这是 NAPI 本身的难点”还是“这是 transport 层的难点”。
