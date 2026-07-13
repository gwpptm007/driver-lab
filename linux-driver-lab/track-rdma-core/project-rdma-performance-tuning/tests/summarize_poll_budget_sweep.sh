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

if [[ ! -f "${MATRIX_CSV}" ]]; then
  echo "missing_poll_budget_matrix path=${MATRIX_CSV}" >&2
  exit 1
fi

awk -F',' -v inline_mode="${SWEEP_USE_INLINE}" '
NR == 1 { next }
NR == 2 {
  best_tp_budget = $1 + 0
  best_tp_batch = $3 + 0
  best_tp = $4 + 0
  best_lat_budget = $1 + 0
  best_lat_batch = $5 + 0
  best_lat = $6 + 0
  best_speed_budget = $1 + 0
  best_speed_batch = $7 + 0
  best_speed = $8 + 0
  rows = 1
  next
}
{
  rows++
  if (($4 + 0) > best_tp) {
    best_tp_budget = $1 + 0
    best_tp_batch = $3 + 0
    best_tp = $4 + 0
  }
  if (($6 + 0) < best_lat) {
    best_lat_budget = $1 + 0
    best_lat_batch = $5 + 0
    best_lat = $6 + 0
  }
  if (($8 + 0) > best_speed) {
    best_speed_budget = $1 + 0
    best_speed_batch = $7 + 0
    best_speed = $8 + 0
  }
}
END {
  if (rows == 0)
    exit 1
  printf("# Poll Budget Sweep Summary\n\n")
  printf("- inline mode: %s\n", inline_mode == "1" ? "on" : "off")
  printf("- rows: %d\n", rows)
  printf("- best throughput budget: %d (`batch_size=%d`, `msg_per_sec=%d`)\n",
         best_tp_budget, best_tp_batch, best_tp)
  printf("- best latency budget: %d (`batch_size=%d`, `batch_avg_msg_ns=%d`)\n",
         best_lat_budget, best_lat_batch, best_lat)
  printf("- best speedup budget: %d (`batch_size=%d`, `speedup_x100=%d`)\n\n",
         best_speed_budget, best_speed_batch, best_speed)
  printf("## Per Budget\n\n")
  printf("| poll_budget | best_tp_batch | best_msg_per_sec | best_lat_batch | best_batch_avg_msg_ns | best_speed_batch | best_speedup_x100 |\n")
  printf("| --- | ---: | ---: | ---: | ---: | ---: | ---: |\n")
}' "${MATRIX_CSV}" > "${OUT_MD}"

tail -n +2 "${MATRIX_CSV}" | awk -F',' '
{
  printf("| %s | %s | %s | %s | %s | %s | %s |\n",
         $1, $3, $4, $5, $6, $7, $8)
}' >> "${OUT_MD}"

echo "poll_budget_summary=pass output=${OUT_MD}"
cat "${OUT_MD}"
