#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
. tests/perf_mode_helpers.sh

SWEEP_USE_INLINE="${SWEEP_USE_INLINE:-0}"
SWEEP_SIGNAL_INTERVAL="${SWEEP_SIGNAL_INTERVAL:-1}"
SWEEP_POLL_CQ_BUDGET="${SWEEP_POLL_CQ_BUDGET:-16}"
SWEEP_CSV="$(perf_sweep_csv_path "${SWEEP_USE_INLINE}" "${SWEEP_SIGNAL_INTERVAL}" "${SWEEP_POLL_CQ_BUDGET}")"
OUT_MD="$(perf_sweep_summary_path "${SWEEP_USE_INLINE}" "${SWEEP_SIGNAL_INTERVAL}" "${SWEEP_POLL_CQ_BUDGET}")"

if [[ ! -f "${SWEEP_CSV}" ]]; then
  echo "missing_sweep_csv path=${SWEEP_CSV}" >&2
  exit 1
fi

awk -F',' -v inline_mode="${SWEEP_USE_INLINE}" -v signal_interval="${SWEEP_SIGNAL_INTERVAL}" -v poll_budget="${SWEEP_POLL_CQ_BUDGET}" '
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
  if (rows == 0) {
    exit 1
  }
  printf("# Perf Sweep Summary\n\n")
  printf("- inline mode: %s\n", inline_mode == "1" ? "on" : "off")
  printf("- signal mode: %s\n", signal_interval == "1" ? "all" : "selective")
  printf("- signal interval: %s\n", signal_interval)
  printf("- poll mode: %s\n", poll_budget == "1" ? "single" : "burst")
  printf("- poll budget: %s\n", poll_budget)
  printf("- rows: %d\n", rows)
  printf("- best throughput batch_size: %d (`msg_per_sec=%d`)\n", best_tp_bs, best_tp)
  printf("- best latency batch_size: %d (`batch_avg_msg_ns=%d`)\n", best_lat_bs, best_lat)
  printf("- best speedup batch_size: %d (`speedup_x100=%d`)\n", best_speed_bs, best_speed)
} ' "${SWEEP_CSV}" > "${OUT_MD}"

echo "sweep_summary=pass output=${OUT_MD}"
cat "${OUT_MD}"
