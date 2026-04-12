# stage04_ring_dma / TASKS

## 目标

- [x] 引入教学型 RX/TX ring descriptor
- [x] 用 ownership/state 表达“CPU / device 谁拥有这个 slot”
- [x] 在 TX 路径引入 `dma_map_single()` / `dma_unmap_single()`
- [x] 在 RX 路径建立“预投递 buffer -> 设备写入 -> CPU 处理 -> refill”
- [x] 保持 `NAPI` 作为 drain 机制
- [x] 补充 `debugfs` 统计与 ring dump
- [x] 形成可执行 smoke 脚本

## 本阶段最重要的验收

不是“性能有多高”，而是：

1. 能解释 RX ring 每个 slot 的生命周期
2. 能看到 refill 确实发生
3. 能区分：
   - TX DMA map/unmap
   - RX buffer 预投递 / 完成 / refill
4. 能从 `debugfs` 看出 ring 深度、drop、budget、refill 行为
