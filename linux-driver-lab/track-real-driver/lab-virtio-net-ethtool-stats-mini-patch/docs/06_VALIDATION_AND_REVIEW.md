# 06_VALIDATION_AND_REVIEW

## 最低通过标准

1. 有一个明确 patch 点
2. 有真实 patch 文件
3. 有 before/after 数据
4. 有 review note
5. 能解释和 `stage12` / `source-dive` / `runtime-observe` 的关系

## 标准通过

在最低通过基础上，再满足：

- 有至少一轮 ping workload
- 有一轮 after 结果
- 有 stats diff
- 有一份面向评审的简明报告

## 优秀通过

- 你能解释为什么选这个 patch 点，而不是更重的主路径点
- 你能说明这个 patch 对后续 tracing / queue-poll 观测的价值
- 你能形成“第一次真实驱动 patch”的完整故事线

## 评审时建议优先看

- `patches/0001-virtio_net-xxx.patch`
- `records/<ts>/PATCH_POINT_NOTE.md`
- `records/<ts>/BEFORE_AFTER.md`
- `records/<ts>/PATCH_REVIEW_NOTE.md`
- `reports/ethtool_patch_report.md`
