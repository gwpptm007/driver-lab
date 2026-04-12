# 03_RX_REPLENISHMENT

## 为什么 RX replenishment 是本阶段核心

这是网络驱动与前面字符设备 / 平台驱动 / 简单 coherent DMA 最大的不同点之一。

### 常见误解

“设备把数据写进一个 buffer，CPU 处理完，再继续用这个 buffer 不就行了吗？”

这在教学理解上很容易产生，但它不符合真实 RX ring 的心智模型。

## 正确理解

一个 RX descriptor 的生命周期大致是：

1. **refill**：驱动分配 fresh `skb`，映射成 `DMA_FROM_DEVICE`，owner=DEV
2. **device write**：设备把报文写进去，owner 从 DEV 转给 CPU
3. **poll consume**：NAPI poll 取出这个 `skb`，送给协议栈
4. **re-post**：原 slot 再补一个 fresh buffer，重新交给 device

### 关键点

- 重点不是“复用旧包”，而是“保持 ring 始终有可用 buffer”
- 如果 refill 跟不上，ring 很快会被耗空
- ring 一旦耗空，RX 就会停摆或开始 drop

## 本阶段怎么体现 replenishment

`netdev_stage04.c` 中：

- 模块初始化时，会先把整个 RX ring 尽量填满
- 每当 poll 消费完一个 done descriptor，就立即调用 `stage04_refill_rx_slot()`
- `debugfs` 会导出：
  - `rx_refill_attempts`
  - `rx_refill_ok`
  - `rx_refill_fail`
  - `rx_ring_posted`
  - `rx_ring_done`
  - `rx_no_desc_drop`

## 验收时最应该看什么

1. `debugfs` 里 refill 计数是否增加
2. ring dump 里 slot 是否从 `DONE -> POSTED` 循环
3. 高 burst 下是否出现 `rx_no_desc_drop`
4. budget 小于 burst 时，是否能观察到分批 drain
