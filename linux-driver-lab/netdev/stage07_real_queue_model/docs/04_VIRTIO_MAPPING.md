# 04_VIRTIO_MAPPING

## 目标

这一阶段不是要重写 `virtio-net`，而是把它最关键的 queue 思想映射进自己的教学驱动。

## 建议映射关系

### 1. TX submit
你的：
- `submit_idx`
- 提交 desc
- notify device

可类比 virtio 的：
- avail ring 推进
- kick device

### 2. TX completion
你的：
- `complete_idx`
- reclaim used buffer

可类比 virtio 的：
- used ring 消费
- 清理已完成 buffer

### 3. RX post
你的：
- `post_idx`
- 预投递可接收 buffer

可类比 virtio 的：
- 向 RX virtqueue 预先挂 buffer

### 4. RX consume
你的：
- `consume_idx`
- 从已完成的 RX slot 取包并上送

可类比 virtio 的：
- 从 used ring 中取回 device 填好的 buffer

### 5. notify / irq / poll
你的：
- doorbell
- irq trigger
- napi poll

可类比 virtio 的：
- kick
- interrupt callback
- napi-driven receive / cleanup

## 为什么这个映射重要

这样你就不是“看懂了 virtio-net 的代码片段”，而是：

> 把 virtio-net 的核心组织思想，翻译成了你自己可控、可解释的模型。

## 本阶段不追求的 virtio 细节

本阶段不要求完整覆盖：
- feature negotiation
- mergeable buffer
- multiqueue
- XDP hooks
- control virtqueue

先把 queue lifecycle 映射清楚更重要。
