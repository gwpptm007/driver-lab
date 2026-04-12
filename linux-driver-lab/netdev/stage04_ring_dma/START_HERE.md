# stage04_ring_dma / START_HERE

## 先看什么

1. `README.md`
2. `docs/02_RING_AND_DMA_MODEL.md`
3. `docs/03_RX_REPLENISHMENT.md`
4. `docs/04_TEST_AND_ACCEPTANCE.md`
5. `driver/netdev_stage04.c`

## 一句话理解本阶段

stage03 已经证明了 `NAPI` 这件事“能工作”。

stage04 要回答的是：

> 如果驱动手里真的有一组 RX/TX descriptor ring，并且 RX buffer 需要不断补充，那整条路径该怎样理解？
