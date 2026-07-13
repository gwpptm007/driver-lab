#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

LEFT_CSV="tests/perf-sweep.csv" \
RIGHT_CSV="tests/perf-sweep-inline.csv" \
LEFT_LABEL="normal" \
RIGHT_LABEL="inline" \
OUT_MD="tests/perf-inline-vs-normal-summary.md" \
REPORT_TITLE="Perf Inline vs Normal Summary" \
  bash tests/compare_sweep_pair.sh
