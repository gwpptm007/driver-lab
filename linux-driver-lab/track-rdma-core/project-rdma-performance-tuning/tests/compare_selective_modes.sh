#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

SELECTIVE_SIGNAL_INTERVAL="${SELECTIVE_SIGNAL_INTERVAL:-4}"

LEFT_CSV="tests/perf-sweep.csv" \
RIGHT_CSV="tests/perf-sweep-sig${SELECTIVE_SIGNAL_INTERVAL}.csv" \
LEFT_LABEL="all-signaled" \
RIGHT_LABEL="selective" \
OUT_MD="tests/perf-selective-vs-all-summary.md" \
REPORT_TITLE="Perf Selective vs All-Signaled Summary" \
  bash tests/compare_sweep_pair.sh
