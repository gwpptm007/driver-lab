#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
. tests/perf_mode_helpers.sh

SUDO_PASSWORD="${SUDO_PASSWORD:-}"
SWEEP_ITERATIONS="${SWEEP_ITERATIONS:-1000}"
SWEEP_BATCH_SIZES="${SWEEP_BATCH_SIZES:-1 2 4 8 16}"
SWEEP_USE_INLINE="${SWEEP_USE_INLINE:-0}"
SIGNAL_INTERVALS="${SIGNAL_INTERVALS:-1 2 4 8 16}"

if [[ "${SWEEP_USE_INLINE}" == "1" ]]; then
  OUT_CSV="tests/perf-signal-interval-inline-sweep.csv"
else
  OUT_CSV="tests/perf-signal-interval-sweep.csv"
fi

echo "signal_interval_sweep_config iterations=${SWEEP_ITERATIONS} batch_sizes=${SWEEP_BATCH_SIZES} inline=${SWEEP_USE_INLINE} intervals=${SIGNAL_INTERVALS}"

{
  echo "signal_interval,inline_mode,best_tp_batch_size,best_msg_per_sec,best_lat_batch_size,best_batch_avg_msg_ns,best_speed_batch_size,best_speedup_x100,source_csv"
} > "${OUT_CSV}"

for signal_interval in ${SIGNAL_INTERVALS}; do
  echo "signal_interval_sweep_step status=start signal_interval=${signal_interval}"

  SWEEP_ITERATIONS="${SWEEP_ITERATIONS}" \
  SWEEP_BATCH_SIZES="${SWEEP_BATCH_SIZES}" \
  SWEEP_USE_INLINE="${SWEEP_USE_INLINE}" \
  SWEEP_SIGNAL_INTERVAL="${signal_interval}" \
  SUDO_PASSWORD="${SUDO_PASSWORD}" \
  bash tests/run_perf_sweep.sh

  SWEEP_CSV="$(perf_sweep_csv_path "${SWEEP_USE_INLINE}" "${signal_interval}")"
  if [[ ! -f "${SWEEP_CSV}" ]]; then
    echo "missing_interval_sweep_csv path=${SWEEP_CSV}" >&2
    exit 1
  fi

  awk -F',' -v signal_interval="${signal_interval}" -v inline_mode="${SWEEP_USE_INLINE}" -v source_csv="${SWEEP_CSV}" '
  NR == 1 { next }
  NR == 2 {
    best_tp_bs = $1 + 0
    best_tp = $20 + 0
    best_lat_bs = $1 + 0
    best_lat = $13 + 0
    best_speed_bs = $1 + 0
    best_speed = $21 + 0
    rows = 1
    next
  }
  {
    rows++
    if (($20 + 0) > best_tp) {
      best_tp = $20 + 0
      best_tp_bs = $1 + 0
    }
    if (($13 + 0) < best_lat) {
      best_lat = $13 + 0
      best_lat_bs = $1 + 0
    }
    if (($21 + 0) > best_speed) {
      best_speed = $21 + 0
      best_speed_bs = $1 + 0
    }
  }
  END {
    if (rows == 0)
      exit 1
    printf("%s,%s,%d,%d,%d,%d,%d,%d,%s\n",
           signal_interval, inline_mode,
           best_tp_bs, best_tp,
           best_lat_bs, best_lat,
           best_speed_bs, best_speed,
           source_csv)
  }' "${SWEEP_CSV}" >> "${OUT_CSV}"

  echo "signal_interval_sweep_step status=done signal_interval=${signal_interval}"
done

echo "signal_interval_sweep=pass output=${OUT_CSV}"
cat "${OUT_CSV}"
