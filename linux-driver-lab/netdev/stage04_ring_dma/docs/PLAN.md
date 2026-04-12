# stage04_ring_dma / PLAN

## 目标
ring / DMA / RX replenishment

## 重点
本阶段是 netdev 主线的核心难点阶段。

## 任务
- 设计 descriptor/ring 结构
- 解释 ownership 与 completion
- 引入 streaming DMA map/unmap
- 把 RX replenishment 作为独立主题做透

## 验收口径
见 `../../docs/06_ACCEPTANCE_AND_MILESTONES.md` 中对应里程碑。
