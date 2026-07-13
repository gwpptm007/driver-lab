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

if [[ ! -f "${MATRIX_CSV}" ]]; then
  echo "missing_signal_interval_matrix path=${MATRIX_CSV}" >&2
  exit 1
fi

awk -F',' -v inline_mode="${SWEEP_USE_INLINE}" '
NR == 1 { next }
NR == 2 {
  best_tp_interval = $1 + 0
  best_tp_batch = $3 + 0
  best_tp = $4 + 0
  best_lat_interval = $1 + 0
  best_lat_batch = $5 + 0
  best_lat = $6 + 0
  best_speed_interval = $1 + 0
  best_speed_batch = $7 + 0
  best_speed = $8 + 0
  rows = 1
  lines[rows] = sprintf("| %s | %s | %s | %s | %s | %s | %s |",
                        $1, $3, $4, $5, $6, $7, $8)
  next
}
{
  rows++
  lines[rows] = sprintf("| %s | %s | %s | %s | %s | %s | %s |",
                        $1, $3, $4, $5, $6, $7, $8)
  if (($4 + 0) > best_tp) {
    best_tp = $4 + 0
    best_tp_interval = $1 + 0
    best_tp_batch = $3 + 0
  }
  if (($6 + 0) < best_lat) {
    best_lat = $6 + 0
    best_lat_interval = $1 + 0
    best_lat_batch = $5 + 0
  }
  if (($8 + 0) > best_speed) {
    best_speed = $8 + 0
    best_speed_interval = $1 + 0
    best_speed_batch = $7 + 0
  }
}
END {
  if (rows == 0)
    exit 1
  printf("# Signal Interval Sweep Summary\n\n")
  printf("- inline mode: %s\n", inline_mode == "1" ? "on" : "off")
  printf("- rows: %d\n", rows)
  printf("- best throughput interval: %d (`batch_size=%d`, `msg_per_sec=%d`)\n",
         best_tp_interval, best_tp_batch, best_tp)
  printf("- best latency interval: %d (`batch_size=%d`, `batch_avg_msg_ns=%d`)\n",
         best_lat_interval, best_lat_batch, best_lat)
  printf("- best speedup interval: %d (`batch_size=%d`, `speedup_x100=%d`)\n",
         best_speed_interval, best_speed_batch, best_speed)
  printf("\n## Per Interval\n\n")
  printf("| signal_interval | best_tp_batch | best_msg_per_sec | best_lat_batch | best_batch_avg_msg_ns | best_speed_batch | best_speedup_x100 |\n")
  printf("| --- | ---: | ---: | ---: | ---: | ---: | ---: |\n")
  for (i = 1; i <= rows; i++)
    print lines[i]
} ' "${MATRIX_CSV}" > "${OUT_MD}"

echo "signal_interval_summary=pass output=${OUT_MD}"
cat "${OUT_MD}"
