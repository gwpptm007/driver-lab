#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

SELECTIVE_SIGNAL_INTERVAL="${SELECTIVE_SIGNAL_INTERVAL:-4}"

LEFT_CSV="tests/perf-sweep-inline.csv" \
RIGHT_CSV="tests/perf-sweep-inline-sig${SELECTIVE_SIGNAL_INTERVAL}.csv" \
LEFT_SUMMARY="tests/perf-sweep-inline-summary.md" \
RIGHT_SUMMARY="tests/perf-sweep-inline-sig${SELECTIVE_SIGNAL_INTERVAL}-summary.md" \
COMPARE_SUMMARY="tests/perf-inline-selective-vs-inline-summary.md" \
LEFT_LABEL="inline" \
RIGHT_LABEL="inline-selective" \
  bash tests/check_compare_pair_report.sh
