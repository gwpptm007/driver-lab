#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

SELECTIVE_SIGNAL_INTERVAL="${SELECTIVE_SIGNAL_INTERVAL:-4}"

LEFT_CSV="tests/perf-sweep.csv" \
RIGHT_CSV="tests/perf-sweep-sig${SELECTIVE_SIGNAL_INTERVAL}.csv" \
LEFT_SUMMARY="tests/perf-sweep-summary.md" \
RIGHT_SUMMARY="tests/perf-sweep-sig${SELECTIVE_SIGNAL_INTERVAL}-summary.md" \
COMPARE_SUMMARY="tests/perf-selective-vs-all-summary.md" \
LEFT_LABEL="all-signaled" \
RIGHT_LABEL="selective" \
  bash tests/check_compare_pair_report.sh
