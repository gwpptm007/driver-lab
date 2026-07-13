#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

# 省额度查看方式：只抓阶段 marker、结果行和清理结果，不展开完整日志。
grep -E \
  'script_config|script_case|PASS:|PERF_SEND_LATENCY|PERF_BATCH_SEND|PERF_BATCH_SEND_.*SELECTIVE|perf_result|perf_throughput|perf_compare|batch_signal_plan|signal_mode|signal_interval|poll_mode|poll_budget|cleanup=complete|post_batch_|batch_buffer_too_small|batch_payload_mismatch' \
  tests/perf-client.log tests/perf-server.log
