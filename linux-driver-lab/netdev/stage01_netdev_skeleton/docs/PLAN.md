# stage01_netdev_skeleton / PLAN

## 目标
最小 net_device 骨架

## 重点
本阶段只聚焦 net_device 生命周期，不引入过多平台复杂度。

## 任务
- 理解 alloc_netdev_mqs / register_netdev / unregister_netdev
- 实现 ndo_open / ndo_stop / ndo_start_xmit 的最小可观测骨架
- 准备最小 stats/debugfs 导出
- 保持架构中立，不写死 ARM64 依赖

## 验收口径
见 `../../docs/06_ACCEPTANCE_AND_MILESTONES.md` 中对应里程碑。
