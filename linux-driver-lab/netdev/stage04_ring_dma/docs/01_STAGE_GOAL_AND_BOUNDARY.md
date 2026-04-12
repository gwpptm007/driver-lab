# 01_STAGE_GOAL_AND_BOUNDARY

## 本阶段到底学什么

stage04 学的是 **ring / DMA / RX replenishment 心智模型**，不是学真实网卡的所有细节。

当前阶段最重要的四个关键词：

- descriptor
- ownership
- streaming DMA
- refill

## 和 stage03 的边界

stage03 重点是：

- 为什么需要 NAPI
- 中断只做 schedule
- poll 按 budget drain pending queue

stage04 重点是：

- pending queue 进一步落成 **descriptor ring**
- RX packet 不是凭空出现，而是先有 **预投递 buffer**
- 处理完一个 RX slot 后，不是“直接复用”，而是 **重新补一个 fresh buffer**

## 本阶段不做的事情

- 不做多队列
- 不做 XDP
- 不做 offload
- 不做真实硬件中断合并
- 不做完整 virtio feature negotiation

这些都会留到后面的 `stage05_virtio_param` 再系统引入。
