#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

SWEEP_USE_INLINE="${SWEEP_USE_INLINE:-0}"

if [[ "${SWEEP_USE_INLINE}" == "1" ]]; then
  MATRIX_CSV="tests/perf-signal-interval-inline-sweep.csv"
  OUT_MD="tests/perf-signal-interval-inline-summary.md"
else
  MATRIX_CSV="tests/perf-signal-interval-sweep.csv"
  OUT_MD="tests/perf-signal-interval-summary.md"
fi

for path in "${MATRIX_CSV}" "${OUT_MD}"; do
  if [[ ! -f "${path}" ]]; then
    echo "missing_signal_interval_artifact path=${path}" >&2
    exit 1
  fi
done

line_count="$(wc -l < "${MATRIX_CSV}")"
if [[ "${line_count}" -lt 2 ]]; then
  echo "invalid_signal_interval_matrix line_count=${line_count}" >&2
  exit 1
fi

for heading in \
  "# Signal Interval Sweep Summary" \
  "## Per Interval"; do
  if ! grep -q "^${heading}$" "${OUT_MD}"; then
    echo "missing_signal_interval_heading heading=${heading}" >&2
    exit 1
  fi
done

echo "signal_interval_check=pass matrix=${MATRIX_CSV} summary=${OUT_MD} lines=${line_count}"
