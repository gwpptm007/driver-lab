#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
. tests/perf_mode_helpers.sh

SWEEP_USE_INLINE="${SWEEP_USE_INLINE:-0}"
SWEEP_SIGNAL_INTERVAL="${SWEEP_SIGNAL_INTERVAL:-1}"
SWEEP_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET:-16}"
SWEEP_CSV="$(perf_sweep_csv_path "${SWEEP_USE_INLINE}" "${SWEEP_SIGNAL_INTERVAL}" "${SWEEP_POLL_CQ_BUDGET}")"

if [[ ! -f "${SWEEP_CSV}" ]]; then
  echo "missing_sweep_csv path=${SWEEP_CSV}" >&2
  exit 1
fi

line_count="$(wc -l < "${SWEEP_CSV}")"
if [[ "${line_count}" -lt 2 ]]; then
  echo "invalid_sweep_csv line_count=${line_count}" >&2
  exit 1
fi

header="$(head -n 1 "${SWEEP_CSV}")"
if [[ "${header}" != batch_size,* ]]; then
  echo "invalid_sweep_header header=${header}" >&2
  exit 1
fi

tail -n +2 "${SWEEP_CSV}" | awk -F',' '
  NF < 10 { exit 1 }
  $1 == "" || $2 == "" || $3 == "" || $4 == "" { exit 1 }
'

echo "sweep_csv_check=pass path=${SWEEP_CSV} lines=${line_count}"
