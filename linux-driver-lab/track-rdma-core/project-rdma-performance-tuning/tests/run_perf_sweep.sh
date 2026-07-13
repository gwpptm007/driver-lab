#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
. tests/perf_mode_helpers.sh

SUDO_PASSWORD="${SUDO_PASSWORD:-}"
SWEEP_ITERATIONS="${SWEEP_ITERATIONS:-1000}"
SWEEP_BATCH_SIZES="${SWEEP_BATCH_SIZES:-1 2 4 8 16}"
SWEEP_USE_INLINE="${SWEEP_USE_INLINE:-0}"
SWEEP_SIGNAL_INTERVAL="${SWEEP_SIGNAL_INTERVAL:-1}"
SWEEP_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET:-16}"
TMP_CSV="tests/perf-summary.csv"
OUT_CSV="$(perf_sweep_csv_path "${SWEEP_USE_INLINE}" "${SWEEP_SIGNAL_INTERVAL}" "${SWEEP_POLL_CQ_BUDGET}")"
SWEEP_DIR="$(perf_sweep_dir_path "${SWEEP_USE_INLINE}" "${SWEEP_SIGNAL_INTERVAL}" "${SWEEP_POLL_CQ_BUDGET}")"

echo "sweep_config iterations=${SWEEP_ITERATIONS} batch_sizes=${SWEEP_BATCH_SIZES} inline=${SWEEP_USE_INLINE} signal_interval=${SWEEP_SIGNAL_INTERVAL} poll_budget=${SWEEP_POLL_CQ_BUDGET}"

bash tests/check_env.sh

rm -f "${OUT_CSV}"
rm -rf "${SWEEP_DIR}"
mkdir -p "${SWEEP_DIR}"

append_header=1
for batch_size in ${SWEEP_BATCH_SIZES}; do
  echo "sweep_step status=start batch_size=${batch_size}"

  PERF_ITERATIONS="${SWEEP_ITERATIONS}" \
  PERF_BATCH_SIZE="${batch_size}" \
  PERF_USE_INLINE="${SWEEP_USE_INLINE}" \
  PERF_SIGNAL_INTERVAL="${SWEEP_SIGNAL_INTERVAL}" \
  PERF_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET}" \
  PERF_SKIP_CLEAN=1 \
  SUDO_PASSWORD="${SUDO_PASSWORD}" \
  bash tests/perf_smoke_test.sh

  bash tests/export_perf_csv.sh >/dev/null

  if [[ ! -f "${TMP_CSV}" ]]; then
    echo "missing_tmp_csv path=${TMP_CSV} batch_size=${batch_size}" >&2
    exit 1
  fi

  if [[ "${append_header}" -eq 1 ]]; then
    {
      printf 'batch_size,'
      head -n 1 "${TMP_CSV}"
      printf '%s,' "${batch_size}"
      tail -n 1 "${TMP_CSV}"
    } > "${OUT_CSV}"
    append_header=0
  else
    printf '%s,%s\n' "${batch_size}" "$(tail -n 1 "${TMP_CSV}")" >> "${OUT_CSV}"
  fi

  cp tests/perf-client.log "${SWEEP_DIR}/batch-${batch_size}-client.log"
  cp tests/perf-server.log "${SWEEP_DIR}/batch-${batch_size}-server.log"
  cp "${TMP_CSV}" "${SWEEP_DIR}/batch-${batch_size}-summary.csv"

  echo "sweep_step status=done batch_size=${batch_size}"
done

SWEEP_USE_INLINE="${SWEEP_USE_INLINE}" SWEEP_SIGNAL_INTERVAL="${SWEEP_SIGNAL_INTERVAL}" SWEEP_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET}" \
  bash tests/check_sweep_csv.sh
SWEEP_USE_INLINE="${SWEEP_USE_INLINE}" SWEEP_SIGNAL_INTERVAL="${SWEEP_SIGNAL_INTERVAL}" SWEEP_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET}" SWEEP_BATCH_SIZES="${SWEEP_BATCH_SIZES}" \
  bash tests/check_sweep_artifacts.sh
