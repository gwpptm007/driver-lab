# Day33 function_graph 解释模板

## 1. 本轮 workload

- run id:
- workload:
- len:
- seed:

## 2. 关键函数链

- `day33_ioctl`
- `day33_do_run_dma`
- `day33_program_dma`
- `day33_wait_dma_idle`
- `day33_irq_handler`

## 3. 我看到的关键 trace 片段

> 在这里粘贴 5~15 行最关键的 trace 片段

## 4. 人话解释

- 入口函数是谁？
- 核心等待点是谁？
- IRQ 是如何证明 DMA 完成的？
- 这条路径和 day31/day32 的 bench 结论是否一致？
