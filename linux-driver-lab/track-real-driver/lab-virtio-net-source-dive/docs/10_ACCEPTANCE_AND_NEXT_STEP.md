# 10_ACCEPTANCE_AND_NEXT_STEP

## 最低通过标准

1. 能说清 `virtio_net` 的整体结构
2. 能讲清 `probe/remove` 的主骨架
3. 能画出 TX 主路径
4. 能画出 RX 主路径
5. 能讲清 queue / NAPI / IRQ 的关系
6. 能给出 `stage00~stage14 ↔ virtio_net` 的映射表

## 标准通过

在“最低通过”之外，再满足：

- 有 `scripts/` 中的最小辅助脚本
- 有至少一轮 `records/<timestamp>/` 阅读记录
- 有 `reports/stage_vs_virtio_net_report.md`
- 有一份“教学驱动 vs 真实驱动”的差异总结

## 优秀通过

在“标准通过”之外，再满足：

- 能明确指出真实驱动里更复杂的对象/生命周期
- 能指出最适合做小 patch 的 1~2 个位置
- 能自然衔接下一个 Lab

## 下一个推荐方向

### 路线 A：继续真实驱动线
- `lab-real-driver-ethtool-stats`
- `lab-real-driver-small-patch`

### 路线 B：切到虚拟化协同线
- `track-virtual-net/lab-virtio-vhost-kick-notify`
