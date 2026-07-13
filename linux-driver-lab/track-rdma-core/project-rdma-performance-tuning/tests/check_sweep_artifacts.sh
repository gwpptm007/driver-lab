#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
. tests/perf_mode_helpers.sh

SWEEP_BATCH_SIZES="${SWEEP_BATCH_SIZES:-1 2 4 8 16}"
SWEEP_USE_INLINE="${SWEEP_USE_INLINE:-0}"
SWEEP_SIGNAL_INTERVAL="${SWEEP_SIGNAL_INTERVAL:-1}"
SWEEP_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET:-16}"
SWEEP_DIR="$(perf_sweep_dir_path "${SWEEP_USE_INLINE}" "${SWEEP_SIGNAL_INTERVAL}" "${SWEEP_POLL_CQ_BUDGET}")"

if [[ ! -d "${SWEEP_DIR}" ]]; then
  echo "missing_sweep_dir path=${SWEEP_DIR}" >&2
  exit 1
fi

for batch_size in ${SWEEP_BATCH_SIZES}; do
  for suffix in client.log server.log summary.csv; do
    path="${SWEEP_DIR}/batch-${batch_size}-${suffix}"
    if [[ ! -f "${path}" ]]; then
      echo "missing_sweep_artifact path=${path}" >&2
      exit 1
    fi
  done
done

echo "sweep_artifacts_check=pass dir=${SWEEP_DIR}"
