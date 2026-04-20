#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# timeline_check.sh — 验证异步链路 timeline
#
# 通过标准：至少有 1 个队列 doorbell_to_backend_ns > 0
# 这证明 backend workfn 被 doorbell 正确触发（异步链路成立）

set -euo pipefail
DIR=${1:-}
[[ -n "$DIR" ]] || { echo "usage: $0 <record-dir>" >&2; exit 1; }
TL="$DIR/debugfs_timeline_after.txt"
[[ -f "$TL" ]] || { echo "missing $TL" >&2; exit 1; }

# 查找 doorbell_to_backend_ns > 0 的队列
found=0
while IFS= read -r line; do
    if echo "$line" | grep -qE 'doorbell_to_backend_ns=[1-9]'; then
        found=$((found + 1))
    fi
done < "$TL"

if [[ "$found" -ge 1 ]]; then
    echo "timeline PASSED: $found queue(s) with doorbell_to_backend_ns > 0"
    grep -E 'doorbell_to_backend_ns=[1-9]' "$TL" | head -5
    exit 0
fi

echo "timeline FAILED: no queues with doorbell_to_backend_ns > 0"
exit 1