# 02_RING_AND_DMA_MODEL

## 一、教学型 TX/RX ring 模型

本阶段驱动用两组数组模拟硬件队列：

- `tx_ring[]`：教学型 TX descriptor ring
- `rx_ring[]`：教学型 RX descriptor ring

每个 descriptor 至少要回答三个问题：

1. 这个 slot 里有没有 buffer
2. 这个 slot 现在归谁（CPU 还是 device）
3. 这个 slot 当前是什么状态（empty / posted / done / busy）

## 二、TX 路径怎么理解

用户态发一帧以后：

1. `ndo_start_xmit()` 收到 `skb`
2. 把 `skb->data` 做一次 `dma_map_single(..., DMA_TO_DEVICE)`
3. 填一个 TX descriptor，表示“设备可以读这块数据了”
4. 教学型“设备”立刻把数据拷贝到一个可用的 RX descriptor buffer 里
5. TX descriptor 完成，`dma_unmap_single()`

这里的重点不是“真 DMA”，而是建立语义：

> TX 不是 CPU 直接把包交给协议栈，而是先经过一层 descriptor + DMA ownership 的转换。

## 三、RX 路径怎么理解

RX ring 不是收到包时才分配 buffer。

正确心智模型是：

1. 驱动提前准备一批 RX buffer
2. 每个 buffer 映射成 `DMA_FROM_DEVICE`
3. ownership 交给 device
4. “设备”写完数据后，把该 slot 交回 CPU
5. poll 线程把这个包送上协议栈
6. 然后**重新补一个 fresh buffer**回这个 slot

## 四、为什么要用 NAPI drain ring

因为真正的重点不是“中断来一个就收一个”，而是：

- ring 里已经积压了一批 done descriptors
- poll 按 budget 一批批处理
- 处理完以后再决定是否 complete

这样才能把 stage03 的 NAPI 教学模型，和 stage04 的 ring 模型连起来。
