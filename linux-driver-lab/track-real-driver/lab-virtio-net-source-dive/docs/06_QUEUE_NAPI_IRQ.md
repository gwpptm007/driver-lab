# 06_QUEUE_NAPI_IRQ

## 目标

把 queue、NAPI、interrupt、notify/kick 的关系真正串起来。

## 这轮要回答的问题

1. `virtio_net` 的 RX/TX queue 是如何成组组织的？
2. queue pair 与 CPU/NAPI 的关系是什么？
3. 中断与 poll 是怎样协作的？
4. notify/kick 与 IRQ completion 分别处在数据路径的哪一侧？
5. 多队列伸缩时，哪些对象是一一对应，哪些是共享的？

## 建议观察维度

### 1. queue 组织
- receive queue
- send queue
- queue pair
- 与 `virtnet_info` 的挂接

### 2. NAPI 组织
- 一个 NAPI context 挂在哪
- 是偏 RX、偏 TX，还是组合处理
- budget 与 poll 结束条件怎么体现

### 3. IRQ / notify
- IRQ 触发后谁 schedule napi
- poll 完成后谁负责 re-enable
- notify/kick 与 guest/host 的边界在哪

## 对照你自己的 stage

这一篇和下面几个阶段关系最强：

- `stage03_napi_poll`
- `stage09_multi_queue_scaling`
- `stage10_msix_per_queue_irq`

## 本篇交付建议

- 一张 queue / napi / irq 对应表
- 一张时序图：中断 -> poll -> completion -> re-enable
- 一段你自己的总结：为什么真实驱动里的“队列模型”比教学驱动更难
