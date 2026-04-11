# Day19 风险矩阵

| 风险类别 | 当前表现 | 影响面 | 当前判断 | 建议动作 |
|---|---|---|---|---|
| 口径统一风险 | D15/D16 主要来自结果文档，D18 来自标准化 records | 影响跨阶段“严格同比”结论 | 中 | 后续补齐 D15/D16 结构化 records |
| rootfs/perf 周期变化 | D18 已进入带 perf 的 rootfs 周期 | 影响 rootfs 体积与 boot 横比解释 | 高 | 报告中保留 caveat；需要时补统一重采样 |
| 平台扩展风险 | 当前验证建立在 arm64 + QEMU virt + 当前 demo 上 | 影响未来扩到 PCIe/virtio/真板时的可复用性 | 中 | 后续新平台 bring-up 时重新做裁剪回归 |
| 观测能力保留风险 | 继续极限裁剪容易误伤 debugfs/ftrace/perf | 影响 W3 学习和分析目标 | 高 | 在 D20/D21 中把 function_graph/perf 继续列为硬验收项 |
| module 数字段不完整 | D15/D16 未显式落 `modules_built_count` | 影响“module 数”维度的完整度 | 中 | 后续补 records 或补构建统计脚本 |
