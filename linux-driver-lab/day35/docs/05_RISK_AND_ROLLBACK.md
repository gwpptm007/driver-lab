# Day35 风险与回滚

## 1. 当前开放风险

### 风险 1：Day33 trace 覆盖仍可继续优化

现象：

- `function_graph` 已开启
- `trace-window.txt` 已存在
- `day33_ioctl` 与 `day33_irq_handler` 已可见
- 但 `day33_do_run_dma / day33_wait_dma_idle` 在当前窗口中未稳定出现

影响：

- 不影响 Day33 基础通过
- 但会影响“关键路径解释”的完整性

建议：

- 继续优化 `set_graph_function` 与 workload 组合
- 针对 `do_run_dma / wait_dma_idle` 增加更聚焦的 workload

### 风险 2：性能数据主要来自 QEMU EDU 教学环境

影响：

- 数据可用于相对比较
- 不应直接外推出真实硬件绝对性能

建议：

- 报告中明确“教学环境 / 仿真后端”限制

## 2. 回滚建议

### 回滚原则

如果 Day35 之后继续新增实验，出现不稳定情况：

1. 优先回退到 Day34 通过版
2. 保留 Day35 报告作为阶段归档
3. 仅在分支上继续推进 Day36 等增量实验

### 当前推荐回滚点

- 功能与稳定性回滚点：Day34 通过版
- 性能对比回滚点：Day32 通过版
- 可观测性参考点：Day33 当前通过版
