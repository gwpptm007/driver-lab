# stage07 用户指南

## 这是什么

这份 stage07 不是“已经验收完成的阶段”，而是：

- 下一阶段的正式目录
- 完整执行计划落地版
- 已经落下第一版可继续扩展的 v1 实现

## 你该怎么用

### 如果你是继续推进开发
按这个顺序：
1. 完成 `driver/netdev_stage07.c` 的 queue lifecycle 主逻辑
2. 把 `scripts/` 做成 build/run/smoke 的最小闭环
3. 先把 stats / debug dump 固定下来
4. 再做 smoke 与验收

### 如果你是拿来评审
先看：
- `docs/01_STAGE_GOAL_AND_BOUNDARY.md`
- `docs/02_QUEUE_MODEL_AND_DATA_STRUCTURES.md`
- `docs/04_VIRTIO_MAPPING.md`
- `docs/06_ACCEPTANCE_AND_MILESTONES.md`
