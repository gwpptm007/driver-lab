# REVIEW_BUNDLE - lab-kprobe-trace-napi-poll

- record_dir: `/home/wq7/workspace/driver-lab/linux-driver-lab/track-ebpf-observability/lab-kprobe-trace-napi-poll/records/20260518-214641-kprobe-trace-napi-poll`
- date: 2026-05-18T21:47:09+08:00

## 文件状态

| file | status |
|---|---|
| ENV_CHECK.txt | DONE |
| NAPI_PROBE_POINTS.txt | DONE |
| NAPI_POLL_KPROBE.log | DONE |
| NAPI_POLL_RETPROBE.log | DONE |
| DRIVER_POLL_OPTIONAL.log | DONE |
| SOFTIRQ_NAPI_CORRELATION.log | DONE |
| COLLECT_STATS.txt | DONE |

## 判定

| item | result |
|---|---|
| PASS_ENV | YES |
| PASS_PROBE_LIST | YES |
| PASS_NAPI_KPROBE | YES |
| PASS_NAPI_RETPROBE | YES |
| DRIVER_POLL_OPTIONAL | YES |
| PASS_SOFTIRQ_CORRELATION | YES |
| TRAFFIC_OR_EVENTS_OBSERVED | YES |

## 结论建议

PASS_NAPI_OBSERVE

## 说明

驱动 poll 观测是 optional。不同内核和驱动符号可能不同，失败时不阻塞最低验收。
