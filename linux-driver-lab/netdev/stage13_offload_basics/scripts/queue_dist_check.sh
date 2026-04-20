#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# queue_dist_check.sh — 验证多队列分发
#
# 通过标准：至少 2 个队列的 tx_submit 增量 > 0（即测试流量打散了）
# 之前的老 bug：只检查 tx_submit > 0（不管增量），导致历史残留流量让检查假 PASS

set -euo pipefail
DIR=${1:-}
[[ -n "$DIR" ]] || { echo "usage: $0 <record-dir>" >&2; exit 1; }
STATS_BEFORE="$DIR/debugfs_stats_before.txt"
STATS_AFTER="$DIR/debugfs_stats_after.txt"
[[ -f "$STATS_AFTER" ]] || { echo "missing $STATS_AFTER" >&2; exit 1; }
[[ -f "$STATS_BEFORE" ]] || { echo "missing $STATS_BEFORE" >&2; exit 1; }

# 提取 qN tx_submit=N 格式，提取队列编号和值
# 格式：q0 tx_submit=9 | q1 tx_submit=5
before_vals=$(grep -oE 'q[0-9]+: tx_submit=[0-9]+' "$STATS_BEFORE" | sed 's/q\([0-9]\+\): tx_submit=//' | sort -n)
after_vals=$(grep -oE 'q[0-9]+: tx_submit=[0-9]+' "$STATS_AFTER" | sed 's/q\([0-9]\+\): tx_submit=//' | sort -n)

before_arr=($before_vals)
after_arr=($after_vals)

if [[ ${#before_arr[@]} -ne ${#after_arr[@]} ]]; then
    echo "queue_dist FAILED: queue count mismatch before=${#before_arr[@]} after=${#after_arr[@]}"
    exit 1
fi

# 计算每个队列的增量
pass_count=0
total=0
deltas=""
for i in "${!before_arr[@]}"; do
    delta=$((${after_arr[$i]} - ${before_arr[$i]}))
    deltas="$deltas $delta"
    if [[ $delta -gt 0 ]]; then
        pass_count=$((pass_count + 1))
    fi
    total=$((total + 1))
done

# 期望至少 2 个队列都有增量（打散）
if [[ "$pass_count" -ge 2 ]] && [[ "$total" -ge 2 ]]; then
    echo "queue_dist PASSED: $pass_count/$total queues with tx_submit delta > 0"
    echo "  tx_submit deltas:$deltas"
    echo "  tx_submit before: $before_vals"
    echo "  tx_submit after:  $after_vals"
    exit 0
fi

echo "queue_dist FAILED: only $pass_count/$total queues with tx_submit delta > 0 (need >= 2)"
echo "  tx_submit deltas:$deltas"
echo "  tx_submit before: $before_vals"
echo "  tx_submit after:  $after_vals"
exit 1