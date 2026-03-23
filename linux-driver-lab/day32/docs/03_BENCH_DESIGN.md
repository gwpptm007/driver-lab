# Day32 workload 设计

## 1. workload 选择

Day32 默认围绕 `len=256` 的 mmap workload 做对比：

- baseline：每轮 `GET_INFO + mmap + munmap`
- optimized：一次 `GET_INFO + mmap`，循环内只跑 memcpy/compare

## 2. 为什么不是先优化 DMA

QEMU EDU 的 DMA 路径里，很多成本来自：

- 设备仿真
- IRQ 往返
- QEMU 调度

这些更适合后续专题。Day32 先找“当天能闭环”的用户态热路径。

## 3. 指标口径

- avg / p50 / p95 / p99 / min / max
- throughput_mbps
- cpu_user_pct / cpu_sys_pct
- success_ops / failed_ops
