#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

CLIENT_LOG="tests/perf-client.log"
OUT_CSV="tests/perf-summary.csv"

if [[ ! -f "${CLIENT_LOG}" ]]; then
  echo "missing_client_log path=${CLIENT_LOG}" >&2
  exit 1
fi

# 从 k=v 日志行里提取字段，避免反复人工抄写 single/batch 指标。
extract_field() {
  local line="$1"
  local key="$2"

  awk -v key="${key}" '
    {
      for (i = 1; i <= NF; i++) {
        split($i, kv, "=")
        if (kv[1] == key) {
          print kv[2]
          exit
        }
      }
    }
  ' <<<"${line}"
}

single_line="$(grep 'perf_result test=send_latency' "${CLIENT_LOG}" | tail -n 1)"
batch_line="$(grep 'perf_result test=batch_send' "${CLIENT_LOG}" | tail -n 1)"
throughput_line="$(grep 'perf_throughput test=batch_send' "${CLIENT_LOG}" | tail -n 1)"
compare_line="$(grep 'perf_compare single_vs_batch' "${CLIENT_LOG}" | tail -n 1)"

if [[ -z "${single_line}" || -z "${batch_line}" || -z "${throughput_line}" || -z "${compare_line}" ]]; then
  echo "missing_perf_markers client_log=${CLIENT_LOG}" >&2
  exit 1
fi

{
  echo "single_iterations,single_avg_ns,single_min_ns,single_p50_ns,single_p95_ns,single_p99_ns,single_max_ns,batch_batches,batch_size,batch_messages,batch_avg_batch_ns,batch_avg_msg_ns,batch_min_batch_ns,batch_p50_batch_ns,batch_p95_batch_ns,batch_p99_batch_ns,batch_max_batch_ns,batch_total_ns,batch_msg_per_sec,speedup_x100,inline_mode,signal_mode,signal_interval,batch_signaled_total,poll_mode,poll_budget"
  printf '%s,%s,%s,%s,%s,%s,%s,' \
    "$(extract_field "${single_line}" iterations)" \
    "$(extract_field "${single_line}" avg_ns)" \
    "$(extract_field "${single_line}" min_ns)" \
    "$(extract_field "${single_line}" p50_ns)" \
    "$(extract_field "${single_line}" p95_ns)" \
    "$(extract_field "${single_line}" p99_ns)" \
    "$(extract_field "${single_line}" max_ns)"
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$(extract_field "${batch_line}" batches)" \
    "$(extract_field "${batch_line}" batch_size)" \
    "$(extract_field "${batch_line}" messages)" \
    "$(extract_field "${batch_line}" avg_batch_ns)" \
    "$(extract_field "${batch_line}" avg_msg_ns)" \
    "$(extract_field "${batch_line}" min_batch_ns)" \
    "$(extract_field "${batch_line}" p50_batch_ns)" \
    "$(extract_field "${batch_line}" p95_batch_ns)" \
    "$(extract_field "${batch_line}" p99_batch_ns)" \
    "$(extract_field "${batch_line}" max_batch_ns)" \
    "$(extract_field "${throughput_line}" total_ns)" \
    "$(extract_field "${throughput_line}" msg_per_sec)" \
    "$(extract_field "${compare_line}" speedup_x100)" \
    "$(extract_field "${compare_line}" inline)" \
    "$(extract_field "${compare_line}" signal_mode)" \
    "$(extract_field "${compare_line}" signal_interval)" \
    "$(extract_field "${batch_line}" signaled_total)" \
    "$(extract_field "${compare_line}" poll_mode)" \
    "$(extract_field "${compare_line}" poll_budget)"
} > "${OUT_CSV}"

echo "csv_export=pass output=${OUT_CSV}"
cat "${OUT_CSV}"
