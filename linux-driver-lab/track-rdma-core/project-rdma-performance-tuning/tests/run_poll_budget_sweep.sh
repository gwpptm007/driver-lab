#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

SUDO_PASSWORD="${SUDO_PASSWORD:-}"
SWEEP_ITERATIONS="${SWEEP_ITERATIONS:-1000}"
SWEEP_BATCH_SIZES="${SWEEP_BATCH_SIZES:-1 2 4 8 16}"
SWEEP_USE_INLINE="${SWEEP_USE_INLINE:-0}"
POLL_CQ_BUDGETS="${POLL_CQ_BUDGETS:-1 2 4 8 16}"

if [[ "${SWEEP_USE_INLINE}" == "1" ]]; then
  OUT_CSV="tests/perf-poll-budget-inline-sweep.csv"
else
  OUT_CSV="tests/perf-poll-budget-sweep.csv"
fi

echo "poll_budget_sweep_config iterations=${SWEEP_ITERATIONS} batch_sizes=${SWEEP_BATCH_SIZES} inline=${SWEEP_USE_INLINE} budgets=${POLL_CQ_BUDGETS}"

{
  echo "poll_budget,inline_mode,best_tp_batch_size,best_msg_per_sec,best_lat_batch_size,best_batch_avg_msg_ns,best_speed_batch_size,best_speedup_x100,source_csv"
} > "${OUT_CSV}"

for poll_budget in ${POLL_CQ_BUDGETS}; do
  echo "poll_budget_sweep_step status=start poll_budget=${poll_budget}"

  SWEEP_ITERATIONS="${SWEEP_ITERATIONS}" \
  SWEEP_BATCH_SIZES="${SWEEP_BATCH_SIZES}" \
  SWEEP_USE_INLINE="${SWEEP_USE_INLINE}" \
  SWEEP_POLL_CQ_BUDGET="${poll_budget}" \
  SUDO_PASSWORD="${SUDO_PASSWORD}" \
  bash tests/run_perf_sweep.sh

  if [[ "${SWEEP_USE_INLINE}" == "1" ]]; then
    if [[ "${poll_budget}" == "16" ]]; then
      SWEEP_CSV="tests/perf-sweep-inline.csv"
    else
      SWEEP_CSV="tests/perf-sweep-inline-poll${poll_budget}.csv"
    fi
  else
    if [[ "${poll_budget}" == "16" ]]; then
      SWEEP_CSV="tests/perf-sweep.csv"
    else
      SWEEP_CSV="tests/perf-sweep-poll${poll_budget}.csv"
    fi
  fi

  if [[ ! -f "${SWEEP_CSV}" ]]; then
    echo "missing_poll_budget_sweep_csv path=${SWEEP_CSV}" >&2
    exit 1
  fi

  awk -F',' -v poll_budget="${poll_budget}" -v inline_mode="${SWEEP_USE_INLINE}" -v source_csv="${SWEEP_CSV}" '
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
           poll_budget, inline_mode,
           best_tp_bs, best_tp,
           best_lat_bs, best_lat,
           best_speed_bs, best_speed,
           source_csv)
  }' "${SWEEP_CSV}" >> "${OUT_CSV}"

  echo "poll_budget_sweep_step status=done poll_budget=${poll_budget}"
done

echo "poll_budget_sweep=pass output=${OUT_CSV}"
cat "${OUT_CSV}"
