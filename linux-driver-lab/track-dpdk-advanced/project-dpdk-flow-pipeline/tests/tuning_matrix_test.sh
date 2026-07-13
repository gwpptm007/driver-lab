#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "${PROJECT_ROOT}"

bash scripts/01_build.sh
bash scripts/03_run_tuning_matrix.sh | tee tests/runtime/tuning_matrix_test.log

CSV=tests/runtime/tuning/TUNING_MATRIX.csv
MD=tests/runtime/tuning/TUNING_MATRIX.md
# 7 个 case 加 1 行 CSV 表头；同时抽查 baseline、最小 burst 和最大规则集。
[[ -s "${CSV}" && -s "${MD}" ]]
[[ $(wc -l <"${CSV}") -eq 8 ]]
grep -q '^baseline,' "${CSV}"
grep -q '^burst_1,' "${CSV}"
grep -q '^rules_512,' "${CSV}"
grep -q 'DPDK_FLOW_PIPELINE_PHASE5_TUNING_PASS' tests/runtime/tuning_matrix_test.log

echo 'PASS: DPDK flow pipeline burst cache rule-count tuning matrix'
echo 'script_summary name=tuning_matrix_test status=pass'
