#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# timeline_check.sh — 验证 stage09 异步链路（doorbell_to_backend_ns > 0）
set -euo pipefail

RECORD_DIR=${1:-.}
TIMELINE_FILE="$RECORD_DIR/debugfs_timeline_after.txt"

if [[ ! -f "$TIMELINE_FILE" ]]; then
    echo "FAIL: $TIMELINE_FILE not found"
    exit 1
fi

# 提取所有队列的 doorbell_to_backend_ns，验证至少有一个 > 0
async_count=$(grep -E '^q[0-9]+:' "$TIMELINE_FILE" | \
    awk -F'doorbell_to_backend_ns=' '{split($2,a," "); if(a[1]+0 > 0) count++} END {print count+0}')

echo "Queues with doorbell_to_backend_ns>0: $async_count"

if [[ "$async_count" -ge 1 ]]; then
    echo "PASS: async backend verified ($async_count queues)"
    exit 0
else
    echo "FAIL: no async backend detected (all doorbell_to_backend_ns == 0)"
    exit 1
fi
