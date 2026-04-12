# 03. stage04 ↔ virtio-net 对照

## 关键认识

- stage04 是“显式化教学模型”
- virtio-net 是“分层后的真实实现”
- 对照重点不是函数一一对应，而是职责和层次对应

## 概念对照

| stage04 教学概念 | virtio-net / vring 对应层 | 说明 |
|---|---|---|
| `tx_ring / rx_ring` | send / receive virtqueue | 都是在表达 buffer 提交与完成 |
| `owner/state` | avail / used ring | 都是在表达 ownership |
| `raise_irq + napi_schedule` | callback / notify / napi schedule | 都是在表达完成事件驱动 poll |
| `refill_rx_slot` | `try_fill_recv` 等补充逻辑 | 都是在解决 RX buffer 不断粮 |
| `dma_map_single/unmap` | transport + sg + DMA 抽象 | stage04 显式化，virtio 分层化 |
