# Day33 function_graph 设计说明

## 1. 为什么选 `function_graph`

`function_graph` 的优势不是“看所有函数”，而是：

- 看调用嵌套
- 看每层函数返回前的持续时间
- 适合把一条关键链路讲清楚

## 2. 为什么要用 `set_graph_function`

如果直接打开 `function_graph`，噪音会非常大。Day33 只保留这些根函数：

- `day33_ioctl`
- `day33_do_run_dma`
- `day33_program_dma`
- `day33_wait_dma_idle`
- `day33_irq_handler`

这样可以把注意力集中在“驱动自己的关键路径”上。

## 3. 为什么默认只 trace 一条小 workload

Day33 的目标是解释路径，不是压测。`mmap-verify` 一次就足够覆盖关键链路，又不会把 trace 拉得过长。
