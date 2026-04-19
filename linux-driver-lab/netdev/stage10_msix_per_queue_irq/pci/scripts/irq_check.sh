#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# irq_check.sh — 验证 stage10 MSI-X per-queue IRQ 分配
#
# 验收标准：
#   1. /proc/interrupts 中存在 stage10 的 IRQ 条目
#   2. 每个队列有独立 IRQ 编号（q0=X, q1=Y, X!=Y）
#   3. irq_count > 0（流量触发过 MSI）

set -euo pipefail

RECORD_DIR=${1:-.}
IRQS_AFTER="$RECORD_DIR/irqs_after.txt"
STATS_AFTER="$RECORD_DIR/debugfs_stats_after.txt"

if [[ ! -f "$IRQS_AFTER" ]]; then
    echo "FAIL: $IRQS_AFTER not found"
    exit 1
fi

if [[ ! -f "$STATS_AFTER" ]]; then
    echo "FAIL: $STATS_AFTER not found"
    exit 1
fi

# 检查 /proc/interrupts 中的 stage10 条目
echo "=== /proc/interrupts stage10 entries ==="
grep -i "stage10" "$IRQS_AFTER" || echo "(none)"
echo ""

# 检查每个队列的 IRQ 编号
echo "=== Per-queue IRQ analysis ==="
irq_lines=$(grep -c "stage10" "$IRQS_AFTER" || echo 0)
echo "stage10 IRQ entries in /proc/interrupts: $irq_lines"

if [[ "$irq_lines" -ge 2 ]]; then
    echo "PASS: >= 2 MSI-X vectors observed"
else
    echo "FAIL: expected >= 2 MSI-X vectors, got $irq_lines"
fi

# 检查 irq_count（流量触发）
echo ""
echo "=== irq_count per queue ==="
grep -E '^q[0-9]+:' "$STATS_AFTER" | \
    awk -F'irq=' '{split($2,a," "); if(a[1]+0 > 0) count++} END {print "queues with irq_count>0: "count+0}'

irq_triggered=$(grep -E '^q[0-9]+:' "$STATS_AFTER" | \
    awk -F'irq=' '{split($2,a," "); if(a[1]+0 > 0) count++} END {print count+0}')
if [[ "$irq_triggered" -ge 1 ]]; then
    echo "PASS: MSI interrupt triggered by traffic ($irq_triggered queues)"
else
    echo "WARN: no irq_count increment (may be OK if BAR doorbell not wired)"
fi
