# virtio_net_round3_key_questions

## Queue / NAPI / IRQ 必须能回答的问题

1. 为什么 queue 不只是数据结构，而是事件推进骨架？
2. 为什么 NAPI 必须放回 callback / schedule / poll 体系理解？
3. callback / IRQ / notify / poll 分别处于什么层次？
4. 为什么真实驱动更适合从“事件推进模型”来读？
5. `stage03/stage04/stage10/stage11` 在这里分别映射到哪里？

## Feature / Offload / XDP 必须能回答的问题

1. 为什么 feature negotiation 是能力契约，而不是一个小步骤？
2. 为什么 offload 更像责任分工，而不是功能名列表？
3. ethtool 为什么更适合归到控制面与能力出口？
4. 为什么 XDP 应该被理解成 RX fast path 边界？
5. `stage12/stage13/stage14` 在这里分别映射到哪里？

## 用途

- 自测
- 评审前自查
- 分享前复盘
