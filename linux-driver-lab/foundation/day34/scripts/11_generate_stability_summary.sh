#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rec="${RECORDS_DIR}/${RUN_ID}"
out="${DAY34_ROOT}/output/day34_stability_summary.md"

ensure_dir "${DAY34_ROOT}/output"

summary_line() {
  local key="$1" file="$2"
  grep -m1 "^${key}=" "$file" 2>/dev/null | sed "s/^${key}=//" || true
}

cat > "$out" <<EOI
# Day34 Stability Summary

- run id: ${RUN_ID}
- concurrent worker_fail: $(summary_line worker_fail "$rec/concurrent-stress.txt")
- module completed_loops: $(summary_line completed_loops "$rec/module-loop.txt") / $(summary_line requested_loops "$rec/module-loop.txt")
- fault invalid len expected_failure: $(summary_line expected_failure "$rec/fault-invalid-len.txt")
- fault mmap offset expected_failure: $(summary_line expected_failure "$rec/fault-mmap-offset.txt")
- run ok marker in mmap-verify: $(grep -m1 '^verify_ok=' "$rec/mmap-verify.txt" 2>/dev/null | cut -d= -f2 || true)

## Overall

请结合 - records/${RUN_ID}/run-summary.md - records/${RUN_ID}/concurrent-stress.txt - records/${RUN_ID}/module-loop.txt - records/${RUN_ID}/fault-invalid-len.txt - records/${RUN_ID}/fault-mmap-offset.txt

来判断 day34 是否通过。
EOI

echo "[day34] 已生成稳定性摘要：$out"
