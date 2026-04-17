# STAGE_OVERVIEW

## stage04 目标与边界

stage04 学的是 **ring / DMA / RX replenishment 心智模型**，不是学真实网卡的所有细节。

当前阶段最重要的四个关键词：

- descriptor
- ownership
- streaming DMA
- refill

---

## 和 stage03 的边界

**stage03 重点是**：
- 为什么需要 NAPI
- 中断只做 schedule
- poll 按 budget drain pending queue

**stage04 重点是**：
- pending queue 进一步落成 **descriptor ring**
- RX packet 不是凭空出现，而是先有 **预投递 buffer**
- 处理完一个 RX slot 后，不是"直接复用"，而是 **重新补一个 fresh buffer**

---

## 本阶段不做的事情

- 不做多队列
- 不做 XDP
- 不做 offload
- 不做真实硬件中断合并
- 不做完整 virtio feature negotiation

这些都会留到后面的 `stage05_virtio_param` 再系统引入。

---

## 一句话理解本阶段

> 如果驱动手里真的有一组 RX/TX descriptor ring，并且 RX buffer 需要不断补充，那整条路径该怎样理解？

---

## 教学型 TX/RX ring 模型

本阶段驱动用两组数组模拟硬件队列：

- `tx_ring[]`：教学型 TX descriptor ring
- `rx_ring[]`：教学型 RX descriptor ring

每个 descriptor 至少要回答三个问题：

1. 这个 slot 里有没有 buffer
2. 这个 slot 现在归谁（CPU 还是 device）
3. 这个 slot 当前是什么状态（empty / posted / done / busy）

---

## TX 路径怎么理解

用户态发一帧以后：

1. `ndo_start_xmit()` 收到 `skb`
2. 把 `skb->data` 做一次 `dma_map_single(..., DMA_TO_DEVICE)`
3. 填一个 TX descriptor，表示"设备可以读这块数据了"
4. 教学型"设备"立刻把数据拷贝到一个可用的 RX descriptor buffer 里
5. TX descriptor 完成，`dma_unmap_single()`

> TX 不是 CPU 直接把包交给协议栈，而是先经过一层 descriptor + DMA ownership 的转换。

---

## RX 路径怎么理解

RX ring 不是收到包时才分配 buffer。

正确心智模型是：

1. 驱动提前准备一批 RX buffer
2. 每个 buffer 映射成 `DMA_FROM_DEVICE`
3. ownership 交给 device
4. "设备"写完数据后，把该 slot 交回 CPU
5. poll 线程把这个包送上协议栈
6. 然后**重新补一个 fresh buffer**回这个 slot

---

## 为什么要用 NAPI drain ring

因为真正的重点不是"中断来一个就收一个"，而是：

- ring 里已经积压了一批 done descriptors
- poll 按 budget 一批批处理
- 处理完以后再决定是否 complete

这样才能把 stage03 的 NAPI 教学模型，和 stage04 的 ring 模型连起来。

---

## 为什么 RX replenishment 是本阶段核心

这是网络驱动与前面字符设备 / 平台驱动 / 简单 coherent DMA 最大的不同点之一。

### 常见误解

"设备把数据写进一个 buffer，CPU 处理完，再继续用这个 buffer 不就行了吗？"

这在教学理解上很容易产生，但它不符合真实 RX ring 的心智模型。

### 正确理解

一个 RX descriptor 的生命周期大致是：

1. **refill**：驱动分配 fresh `skb`，映射成 `DMA_FROM_DEVICE`，owner=DEV
2. **device write**：设备把报文写进去，owner 从 DEV 转给 CPU
3. **poll consume**：NAPI poll 取出这个 `skb`，送给协议栈
4. **re-post**：原 slot 再补一个 fresh buffer，重新交给 device

### 关键点

- 重点不是"复用旧包"，而是"保持 ring 始终有可用 buffer"
- 如果 refill 跟不上，ring 很快会被耗空
- ring 一旦耗空，RX 就会停摆或开始 drop

---

## 与 stage05 的关系

- **stage05**：virtio-net 源码对照 + 平台参数化准备
- **stage06**：ARM64 跨平台迁移收口

```
stage04（教学模型）                    stage05/virtio-net（真实实现）
─────────────────────────────────      ─────────────────────────────────
单文件 driver/netdev_stage04.c         drivers/net/virtio_net.c
├─ 单一 tx_ring / rx_ring 数组         ├─ send_queue / receive_queue + virtqueue
├─ owner/state 显式字段管理             ├─ avail_idx / used_idx ring 协议
├─ dma_map_single 每包显式映射         ├─ virtio transport 隐藏 DMA 细节
└─ refill_rx_slot 同步在 poll 内        └─ try_fill_recv 批量异步填充
```
