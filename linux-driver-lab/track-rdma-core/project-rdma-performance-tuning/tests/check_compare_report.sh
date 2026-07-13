#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

LEFT_CSV="tests/perf-sweep.csv" \
RIGHT_CSV="tests/perf-sweep-inline.csv" \
LEFT_SUMMARY="tests/perf-sweep-summary.md" \
RIGHT_SUMMARY="tests/perf-sweep-inline-summary.md" \
COMPARE_SUMMARY="tests/perf-inline-vs-normal-summary.md" \
LEFT_LABEL="normal" \
RIGHT_LABEL="inline" \
  bash tests/check_compare_pair_report.sh
