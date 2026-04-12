# stage03_napi_poll / PLAN

## 目标
把 stage02 的“立即 RX 注入”升级成：
- pending queue
- NAPI schedule
- poll drain
- budget / complete / 中断抑制

## 教学重点
- 为什么 NAPI 不是“多一个收包 API”
- pending queue 为什么能承载“中断到了但包还没处理完”
- `budget` 限制的不是总包数，而是**本次 poll 最多做多少工作**
- 为什么 `napi_complete_done()` 后才重新开“中断”

## 阶段边界
不做：
- 真实 descriptor ring
- DMA mapping
- RX replenishment
- 多队列 / MSI-X

这些放到 stage04 / stage05。
