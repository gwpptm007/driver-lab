# Day33 trace 阅读指南

读 `trace-window.txt` 时，建议按下面顺序：

1. 找到 `day33_ioctl`
2. 看它向下调用了哪些函数
3. 重点关注 `day33_do_run_dma`
4. 观察两次 `day33_program_dma`
5. 看 `day33_wait_dma_idle` 的持续时间
6. 结合 `irq_handler` 判断设备完成点

## 你通常会看到什么

- `day33_ioctl` 作为入口
- `day33_do_run_dma` 把一次 workload 分成两段 DMA
- 两次 `day33_program_dma`
- `day33_wait_dma_idle` 里体现设备等待成本
- `day33_irq_handler` 作为完成路径的证据

## 如何和 day31/day32 互相印证

- 如果 `wait_dma_idle` 很显眼，说明设备等待仍是主成本
- 如果 `ioctl` 自身非常短，说明 syscall 入口不是主瓶颈
- trace 和 day32 的 `bench-dma-lite` 数据应能形成一致解释
