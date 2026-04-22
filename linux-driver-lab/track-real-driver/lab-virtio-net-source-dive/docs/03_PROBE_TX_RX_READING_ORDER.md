# 03_PROBE_TX_RX_READING_ORDER

## 第一轮：从 probe/remove 入手

建议顺序：

1. 找 `virtio_driver` 注册点
2. 看 `probe()`
3. 理清 netdev 分配、初始化与注册
4. 看 queue 初始化、virtqueue 建立、napi 挂接
5. 看 `remove()` 收尾

## 第二轮：看 TX
- `ndo_start_xmit` 从哪里开始
- skb 什么时候被放入 virtqueue
- 什么动作会触发 notify/kick
- completion/reclaim 在哪里回来

## 第三轮：看 RX
- receive buffer 什么时候预投递
- 收包后如何唤醒 poll
- `virtnet_poll()` 中做了哪些事
- skb 是怎么构建出来的
- GRO / checksum / XDP 的入口在哪里
