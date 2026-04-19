#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# queue_dist_check.sh — 验证 stage09 多队列分布（>= 2 队列有 tx_submit > 0）
set -euo pipefail

RECORD_DIR=${1:-.}
STATS_FILE="$RECORD_DIR/debugfs_stats_after.txt"

if [[ ! -f "$STATS_FILE" ]]; then
    echo "FAIL: $STATS_FILE not found"
    exit 1
fi

# 统计有多少个队列的 tx_submit > 0
active_queues=$(grep -E '^q[0-9]+:' "$STATS_FILE" | \
    awk -F'tx_submit=' '{split($2,a," "); if(a[1]+0 > 0) count++} END {print count+0}')

echo "Active queues with tx_submit>0: $active_queues"

if [[ "$active_queues" -ge 2 ]]; then
    echo "PASS: multi-queue distribution verified ($active_queues queues)"
    exit 0
else
    echo "FAIL: expected >=2 active queues, got $active_queues"
    exit 1
fi
