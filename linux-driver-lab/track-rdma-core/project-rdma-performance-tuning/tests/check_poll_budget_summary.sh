#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

SWEEP_USE_INLINE="${SWEEP_USE_INLINE:-0}"

if [[ "${SWEEP_USE_INLINE}" == "1" ]]; then
  MATRIX_CSV="tests/perf-poll-budget-inline-sweep.csv"
  OUT_MD="tests/perf-poll-budget-inline-summary.md"
else
  MATRIX_CSV="tests/perf-poll-budget-sweep.csv"
  OUT_MD="tests/perf-poll-budget-summary.md"
fi

for path in "${MATRIX_CSV}" "${OUT_MD}"; do
  if [[ ! -f "${path}" ]]; then
    echo "missing_poll_budget_artifact path=${path}" >&2
    exit 1
  fi
done

line_count="$(wc -l < "${MATRIX_CSV}")"
if [[ "${line_count}" -lt 2 ]]; then
  echo "invalid_poll_budget_matrix line_count=${line_count}" >&2
  exit 1
fi

for heading in "# Poll Budget Sweep Summary" "## Per Budget" "| poll_budget |"; do
  if ! grep -q "${heading}" "${OUT_MD}"; then
    echo "missing_poll_budget_heading heading=${heading}" >&2
    exit 1
  fi
done

echo "poll_budget_check=pass matrix=${MATRIX_CSV} summary=${OUT_MD} lines=${line_count}"
