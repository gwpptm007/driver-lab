# 为什么 W5 切到 DMA-capable 后端

## day29 开始的重点
- `dma_alloc_coherent`
- DMA 地址与 CPU 虚拟地址区分
- `mmap`
- bench / perf / ftrace
- 并发与稳定性

## 为什么不继续用 W4 的 ivshmem
- W4 目标已经完成：PCI 基本功、BAR、消息中断、用户态接口
- W5 重点是“真 DMA”，不应该继续用共享内存模型硬撑

## 当前推荐
- QEMU EDU 作为 W5 推荐后端
