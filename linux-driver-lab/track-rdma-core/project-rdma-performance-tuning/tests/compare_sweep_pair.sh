#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

LEFT_CSV="${LEFT_CSV:?missing LEFT_CSV}"
RIGHT_CSV="${RIGHT_CSV:?missing RIGHT_CSV}"
LEFT_LABEL="${LEFT_LABEL:?missing LEFT_LABEL}"
RIGHT_LABEL="${RIGHT_LABEL:?missing RIGHT_LABEL}"
OUT_MD="${OUT_MD:?missing OUT_MD}"
REPORT_TITLE="${REPORT_TITLE:-Perf Sweep Pair Summary}"

if [[ ! -f "${LEFT_CSV}" ]]; then
  echo "missing_left_sweep path=${LEFT_CSV}" >&2
  exit 1
fi
if [[ ! -f "${RIGHT_CSV}" ]]; then
  echo "missing_right_sweep path=${RIGHT_CSV}" >&2
  exit 1
fi

best_line() {
  local csv="$1"
  local field_index="$2"
  local mode="$3"

  awk -F',' -v field_index="${field_index}" -v mode="${mode}" '
  NR == 1 { next }
  NR == 2 {
    best_bs = $1 + 0
    best_value = $field_index + 0
    rows = 1
    next
  }
  {
    rows++
    current = $field_index + 0
    if ((field_index == 13 && current < best_value) ||
        (field_index != 13 && current > best_value)) {
      best_value = current
      best_bs = $1 + 0
    }
  }
  END {
    if (rows == 0)
      exit 1
    printf("%s,%s,%s\n", mode, best_bs, best_value)
  }' "${csv}"
}

left_tp="$(best_line "${LEFT_CSV}" 20 "${LEFT_LABEL}")"
right_tp="$(best_line "${RIGHT_CSV}" 20 "${RIGHT_LABEL}")"
left_lat="$(best_line "${LEFT_CSV}" 13 "${LEFT_LABEL}")"
right_lat="$(best_line "${RIGHT_CSV}" 13 "${RIGHT_LABEL}")"
left_speed="$(best_line "${LEFT_CSV}" 21 "${LEFT_LABEL}")"
right_speed="$(best_line "${RIGHT_CSV}" 21 "${RIGHT_LABEL}")"

{
  echo "# ${REPORT_TITLE}"
  echo
  echo "## 1. Best Throughput"
  echo
  echo "| mode | batch_size | msg_per_sec |"
  echo "| --- | ---: | ---: |"
  echo "| ${left_tp//,/ | } |"
  echo "| ${right_tp//,/ | } |"
  echo
  echo "## 2. Best Latency"
  echo
  echo "\`batch_avg_msg_ns\` 越小越好。"
  echo
  echo "| mode | batch_size | batch_avg_msg_ns |"
  echo "| --- | ---: | ---: |"
  echo "| ${left_lat//,/ | } |"
  echo "| ${right_lat//,/ | } |"
  echo
  echo "## 3. Best Speedup"
  echo
  echo "\`speedup_x100=250\` 表示约 2.50x。"
  echo
  echo "| mode | batch_size | speedup_x100 |"
  echo "| --- | ---: | ---: |"
  echo "| ${left_speed//,/ | } |"
  echo "| ${right_speed//,/ | } |"
  echo
  echo "## 4. Notes"
  echo
  echo "- ${LEFT_LABEL} 数据来自 \`${LEFT_CSV}\`。"
  echo "- ${RIGHT_LABEL} 数据来自 \`${RIGHT_CSV}\`。"
  echo "- 该报告只提取两个模式各自的最优点位，完整 sweep 仍以原始 CSV 和逐 batch 日志为准。"
} > "${OUT_MD}"

echo "sweep_pair_compare=pass output=${OUT_MD} left=${LEFT_LABEL} right=${RIGHT_LABEL}"
cat "${OUT_MD}"
