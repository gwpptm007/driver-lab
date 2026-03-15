# day22 为什么选 ivshmem-doorbell

## 1. 对 W4 很合适

因为它同时具备这几个特点：

- 它是标准 PCI 设备，可被 `lspci` 枚举
- 它有 BAR0/BAR1/BAR2，后续方便讲 BAR/MMIO/共享区
- `doorbell` 版本后续能自然衔接中断实验

## 2. 对 day22 尤其合适

day22 还没开始写驱动，所以更适合选一个：

- 设备模型清晰
- QEMU 上好挂载
- 后面还能延续到 day23/day24/day25

## 3. 为什么 day22 不讨论 DMA

因为这一天的唯一目标是“设备枚举成功”。
DMA 留到 W5 再换 DMA-capable 后端去做，会更正、更清晰。
