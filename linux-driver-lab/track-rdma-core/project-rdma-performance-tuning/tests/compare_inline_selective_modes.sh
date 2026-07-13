#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

SELECTIVE_SIGNAL_INTERVAL="${SELECTIVE_SIGNAL_INTERVAL:-4}"

LEFT_CSV="tests/perf-sweep-inline.csv" \
RIGHT_CSV="tests/perf-sweep-inline-sig${SELECTIVE_SIGNAL_INTERVAL}.csv" \
LEFT_LABEL="inline" \
RIGHT_LABEL="inline-selective" \
OUT_MD="tests/perf-inline-selective-vs-inline-summary.md" \
REPORT_TITLE="Perf Inline-Selective vs Inline Summary" \
  bash tests/compare_sweep_pair.sh
