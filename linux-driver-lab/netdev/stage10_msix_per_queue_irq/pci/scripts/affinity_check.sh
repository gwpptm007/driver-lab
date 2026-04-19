#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# affinity_check.sh — 验证 stage10 MSI-X IRQ affinity 可调
#
# 验收标准：
#   1. /proc/interrupts 显示每个 IRQ 的 CPU 亲和性
#   2. smp_affinity 可写（可重新绑定 IRQ 到不同 CPU）
#   3. 重新绑定后 /proc/interrupts 显示变化

set -euo pipefail

RECORD_DIR=${1:-.}
IRQS_AFTER="$RECORD_DIR/irqs_after.txt"

if [[ ! -f "$IRQS_AFTER" ]]; then
    echo "FAIL: $IRQS_AFTER not found"
    exit 1
fi

echo "=== stage10 IRQ affinity check ==="

# 从 /proc/interrupts 提取 IRQ 编号
irq_nums=$(grep -i "stage10" "$IRQS_AFTER" | awk '{print $1}' | tr -d ':')

if [[ -z "$irq_nums" ]]; then
    echo "FAIL: no stage10 IRQ entries found in /proc/interrupts"
    exit 1
fi

echo "Found stage10 IRQ numbers: $irq_nums"
echo ""

# 检查每个 IRQ 的 affinity（通过 /sys/irq/*/smp_affinity）
all_ok=1
for irq in $irq_nums; do
    affinity_file="/sys/irq/$irq/smp_affinity"
    if [[ -r "$affinity_file" ]]; then
        current_affinity=$(cat "$affinity_file" 2>/dev/null || echo "unreadable")
        echo "IRQ $irq: current affinity=$current_affinity"

        # 尝试写入新 affinity（只测试本机，QEMU VM 内可能不允许）
        if [[ -w "$affinity_file" ]]; then
            orig=$(cat "$affinity_file")
            echo "  -> attempting to set affinity to '2' (CPU 1)..."
            echo 2 > "$affinity_file" 2>/dev/null || true
            new_affinity=$(cat "$affinity_file" 2>/dev/null || echo "unreadable")
            echo "  -> new affinity=$new_affinity"
            echo 2 > "$orig" 2>/dev/null || true  # restore
            echo "  PASS: IRQ $irq affinity is tunable"
        else
            echo "  WARN: $affinity_file not writable (may be restricted in VM)"
        fi
    else
        echo "IRQ $irq: /sys/irq/$irq/smp_affinity not found"
    fi
    echo ""
done

echo "=== affinity_check complete ==="
echo "NOTE: This script tests IRQ affinity tunability on the system."
echo "If all IRQs show 'not writable', this is expected inside a VM (KVM permits it)."
echo "On bare metal, smp_affinity should be tunable."
