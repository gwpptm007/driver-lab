#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rec="${RECORDS_DIR}/${RUN_ID}"
out="${DAY33_ROOT}/output/day33_trace_summary.md"
trace_file="$rec/trace-window.txt"
summary_file="$rec/run-summary.md"

require_file "$summary_file" run-summary.md

sample="(trace-window.txt 尚未生成)"
if [ -f "$trace_file" ]; then
  sample="$(sed -n '1,40p' "$trace_file")"
fi

cat > "$out" <<EOF
# Day33 Trace Summary

## 1. 本轮结果摘要

$(cat "$summary_file")

## 2. trace 前 40 行样本（若 trace 配置失败，这里会显示跳过原因）

\`\`\`
$sample
\`\`\`
EOF

echo "[day33] 已生成 trace 摘要：$out"
