#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "${PROJECT_ROOT}"

rm -rf tests/runtime
bash scripts/01_build.sh
bash scripts/02_run_pcap_smoke.sh
LOG=tests/runtime/flow_pipeline.log

# 先验收建表与 capability，再验证动作计数、生命周期、多 worker 和清理路径。
grep -q 'FLOW_HASH_CREATE_PASS' "${LOG}"
grep -q 'FLOW_RULE_LOAD_PASS count=3' "${LOG}"
grep -q 'FLOW_PORT_CAPABILITY max_rx_queues=' "${LOG}"
grep -Eq 'RTE_FLOW_(VALIDATE_PASS|BOUNDARY_BLOCKED)' "${LOG}"
grep -q 'FLOW_RESULT hash_hits=48 hash_misses=16 rule_drop=16 forward=16 mark=16 default_drop=16 invalid=0' "${LOG}"
grep -q 'FLOW_LATENCY samples=64 ' "${LOG}"
grep -q 'FLOW_RULE\[0\] action=DROP mark=0 packets=16 ' "${LOG}"
grep -q 'FLOW_RULE\[1\] action=MARK mark=42 packets=16 ' "${LOG}"
grep -q 'FLOW_RULE\[2\] action=FORWARD mark=0 packets=16 ' "${LOG}"
grep -q 'APP_RESULT rx=64 tx=32 tx_failed=0 freed=32' "${LOG}"
grep -q 'DPDK_FLOW_PIPELINE_PHASE1_PASS' "${LOG}"
grep -q 'FLOW_RULE_ADD_UPDATE_DELETE_PASS' "${LOG}"
grep -q 'FLOW_RULE_AGING_PASS' "${LOG}"
grep -q 'DPDK_FLOW_PIPELINE_PHASE2_LIFECYCLE_PASS' "${LOG}"
grep -q 'FLOW_WORKER_RESULT mode=shared queue0=32 queue1=32 hits=48 misses=16 drop=16 forward=16 mark=16 default_drop=16' "${LOG}"
grep -q 'FLOW_WORKER_RESULT mode=sharded queue0=32 queue1=32 hits=48 misses=16 drop=16 forward=16 mark=16 default_drop=16' "${LOG}"
grep -q 'FLOW_WORKER_SHARED_TABLE_PASS' "${LOG}"
grep -q 'FLOW_WORKER_SHARDED_TABLE_PASS' "${LOG}"
grep -q 'DPDK_FLOW_PIPELINE_PHASE3_WORKER_PASS' "${LOG}"
grep -q 'cleanup=complete result=pass' "${LOG}"

echo 'PASS: DPDK rte_hash flow actions and latency smoke'
echo 'script_summary name=flow_pipeline_smoke status=pass'
