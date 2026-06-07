# REVIEW_BUNDLE - lab-tracepoint-skb-path

- record_dir: `/home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-tracepoint-skb-path/records/20260606-161506-tracepoint-skb-path`
- date: 2026-06-06T16:16:50+08:00

## 文件状态

| file | status |
|---|---|
| ENV_CHECK.txt | DONE |
| TRACEPOINT_LIST.txt | DONE |
| SKB_RX_TRACE.log | DONE |
| SKB_TX_TRACE.log | DONE |
| SKB_DROP_TRACE.log | DONE |
| SKB_FULL_PATH.log | DONE |
| COLLECT_STATS.txt | DONE |

## 判定

| item | result |
|---|---|
| PASS_ENV | YES |
| PASS_TRACEPOINT_LIST | YES |
| PASS_RX_TRACE | YES |
| PASS_TX_TRACE | YES |
| PASS_DROP_TRACE | YES |
| PASS_FULL_PATH | YES |
| TRAFFIC_OR_EVENTS_OBSERVED | YES |

## tracepoint vs kprobe 对照说明

| 维度 | kprobe | tracepoint |
|---|---|---|
| 稳定性 | 函数名变化导致不兼容 | 内核 ABI，跨版本稳定 |
| 字段访问 | 需要知道结构体布局 | args->字段 可直接访问 |
| 适用场景 | 无 tracepoint 的深层路径 | skb 层面首选 tracepoint |
| 上游保证 | 无 | tracepoint 变更视为 ABI break |

## 结论建议

PASS_SKB_TRACEPOINT_OBSERVE

## 说明

drop trace (kfree_skb) 和 full-path 合并观测是加分项。RX+TX 双通是最低验收。
Phase 3 相比 Phase 2 的进步：从 kprobe 函数级观测 → tracepoint ABI 级观测，稳定性大幅提升。
