# 03_NOTIFY_IRQ_NAPI_COMPLETION

## 阶段核心问题

真实驱动最容易讲乱的，不是 ring 本身，而是：

- 提交后什么时候通知 device
- device 完成后什么时候触发 irq
- irq 做多少事
- NAPI poll 做多少事
- TX completion 和 RX consume 的边界在哪里

## 建议路径分解

### TX submit path
1. netdev xmit 收到 skb
2. 分配/检查 TX queue slot
3. map DMA / 填 desc
4. 推进 `submit_idx`
5. 调用 notify/doorbell

### device progress path
1. 后端模型推进 queue
2. 标记已完成 descriptor
3. 准备触发 completion
4. 触发 irq 或 schedule napi

### IRQ path
建议只做：
- ack/计数
- 关闭中断或抑制重复触发
- `napi_schedule()`

不建议在 irq 里直接做复杂包处理。

### NAPI poll path
建议做：
- TX completion 回收
- RX consume 上送
- RX refill
- budget 判断
- 条件满足时 `napi_complete_done()` 并 re-enable irq

## 为什么这样设计

因为这更接近真实网络驱动的分工：

- irq：只负责“叫醒”
- poll：负责“批处理”
- queue helper：负责“推进状态机”

## 建议统计项

- `irq_count`
- `napi_schedule_count`
- `napi_poll_count`
- `napi_budget_exhaust_count`
- `tx_complete_count`
- `rx_consume_count`
- `rx_refill_count`

## 一句话原则

> irq 只负责触发，poll 负责批处理，queue helper 负责状态推进。
