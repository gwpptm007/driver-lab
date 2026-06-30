# 03_ACCEPTANCE

## Track 级验收

这条 track 的验收不是“某一条命令输出好看”，而是证据链完整：

- 能说明当前机器是否支持真实 RDMA。
- 能说明是否能用 Soft-RoCE 继续 verbs 学习。
- 能跑出至少一个 verbs object lifecycle 实验。
- 能把 RDMA path 和 DPDK fastpath 对照讲清楚。

## Phase 1 验收

| 验收项 | 通过标准 |
| --- | --- |
| 环境采集 | 有 `ENV_CHECK.log` |
| 工具检查 | 有 `RDMA_CAPABILITY.log`，包含命令存在性和实际输出 |
| 设备检查 | 明确 `ibv_devices` / `rdma dev` 是否发现设备 |
| Soft-RoCE 边界 | 明确 `rdma_rxe` 是否存在，是否执行了 setup |
| 总结 | 有 `SUMMARY.md` 和 phase report |

## 结果分类

| 状态 | 含义 |
| --- | --- |
| `PASS_RDMA_DEVICE_PRESENT` | 发现真实 RDMA verbs 设备 |
| `BLOCKED_NO_RDMA_DEVICE` | 未发现 RDMA 设备 |
| `PASS_RDMA_TOOLS_PRESENT` | verbs/rdma 工具存在 |
| `BLOCKED_RDMA_TOOLS_MISSING` | 工具缺失，需要安装 `rdma-core` / `ibverbs-utils` |
| `PASS_SOFT_ROCE_AVAILABLE` | 内核存在 `rdma_rxe` 能力 |
| `BLOCKED_SOFT_ROCE_UNAVAILABLE` | 当前内核没有 Soft-RoCE 模块 |

## 不允许的结论

- 不能因为脚本退出码为 0 就说 RDMA 跑通。
- 不能把 Soft-RoCE 结果说成硬件 RDMA 性能结果。
- 不能在没有 CQE 证据时说 verbs data path 成功。
