# stage01_netdev_skeleton

## 阶段定位
最小 net_device 骨架

## 阶段说明
本阶段只聚焦 net_device 生命周期，不引入过多平台复杂度。

## 核心任务
- [ ] 理解 alloc_netdev_mqs / register_netdev / unregister_netdev
- [ ] 实现 ndo_open / ndo_stop / ndo_start_xmit 的最小可观测骨架
- [ ] 准备最小 stats/debugfs 导出
- [ ] 保持架构中立，不写死 ARM64 依赖

## 当前状态
本阶段当前只落了目录骨架与说明文档，后续将按阶段逐步实现。
