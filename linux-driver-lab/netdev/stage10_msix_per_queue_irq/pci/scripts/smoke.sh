#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# smoke.sh — stage10 PCI + MSI-X smoke test
#
# 验证：
#   1. 多队列分布（queue_dist_check）
#   2. 异步链路成立（timeline_check）
#   3. MSI-X IRQ 可观测（irq_check）
#   4. MSI-X vector 处理（vector_check）

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PCI_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
TOOLS_DIR="$PCI_DIR/tools"
IFNAME=${IFNAME:-nds10}
PROTO=${PROTO:-0x88BA}
COUNT=${COUNT:-64}
RECORD_DIR="$PCI_DIR/records/$(date +%Y%m%d-%H%M%S)-stage10-pci-smoke"
DBG_DIR=${DBG_DIR:-/sys/kernel/debug/netdev_stage10}
mkdir -p "$RECORD_DIR"

sudo -n true >/dev/null 2>&1 || {
    echo "[stage10_pci] sudo required but not available" >&2
    exit 1
}

PASS=1

# capture before
if [[ -d "$DBG_DIR" ]]; then
    sudo cat "$DBG_DIR/stats" > "$RECORD_DIR/debugfs_stats_before.txt" 2>/dev/null || true
    sudo cat "$DBG_DIR/queues" > "$RECORD_DIR/debugfs_queues_before.txt" 2>/dev/null || true
    sudo cat "$DBG_DIR/timeline" > "$RECORD_DIR/debugfs_timeline_before.txt" 2>/dev/null || true
    sudo cat "$DBG_DIR/vectors" > "$RECORD_DIR/debugfs_vectors_before.txt" 2>/dev/null || true
    sudo cat "$DBG_DIR/irqs" > "$RECORD_DIR/debugfs_irqs_before.txt" 2>/dev/null || true
fi
if [[ -r /proc/interrupts ]]; then
    sudo cat /proc/interrupts | grep -E "stage10|MSI" > "$RECORD_DIR/irqs_before.txt" 2>/dev/null || true
fi
ip link show dev "$IFNAME" > "$RECORD_DIR/ip_link_before.txt" 2>/dev/null || true

# send traffic
"$TOOLS_DIR/send_stage10_frame" --ifname "$IFNAME" --proto "$PROTO" --count "$COUNT" \
    > "$RECORD_DIR/send.txt" 2>&1 || true
"$TOOLS_DIR/recv_stage10_frame" --ifname "$IFNAME" --proto "$PROTO" --timeout-sec 3 --count 1 \
    > "$RECORD_DIR/recv.txt" 2>&1 || true

sleep 1

# capture after
if [[ -d "$DBG_DIR" ]]; then
    sudo cat "$DBG_DIR/stats" > "$RECORD_DIR/debugfs_stats_after.txt" 2>/dev/null || true
    sudo cat "$DBG_DIR/queues" > "$RECORD_DIR/debugfs_queues_after.txt" 2>/dev/null || true
    sudo cat "$DBG_DIR/timeline" > "$RECORD_DIR/debugfs_timeline_after.txt" 2>/dev/null || true
    sudo cat "$DBG_DIR/vectors" > "$RECORD_DIR/debugfs_vectors_after.txt" 2>/dev/null || true
    sudo cat "$DBG_DIR/irqs" > "$RECORD_DIR/debugfs_irqs_after.txt" 2>/dev/null || true
fi
if [[ -r /proc/interrupts ]]; then
    sudo cat /proc/interrupts | grep -E "stage10|MSI" > "$RECORD_DIR/irqs_after.txt" 2>/dev/null || true
fi
ip link show dev "$IFNAME" > "$RECORD_DIR/ip_link_after.txt" 2>/dev/null || true
sudo dmesg | tail -n 200 > "$RECORD_DIR/dmesg_tail.txt" 2>/dev/null || true

# run checks
"$SCRIPT_DIR/queue_dist_check.sh" "$RECORD_DIR" > "$RECORD_DIR/queue_dist_check.txt" 2>&1 || PASS=0
"$SCRIPT_DIR/timeline_check.sh" "$RECORD_DIR" > "$RECORD_DIR/timeline_check.txt" 2>&1 || PASS=0
"$SCRIPT_DIR/irq_check.sh" "$RECORD_DIR" > "$RECORD_DIR/irq_check.txt" 2>&1 || PASS=0
"$SCRIPT_DIR/vector_check.sh" "$RECORD_DIR" > "$RECORD_DIR/vector_check.txt" 2>&1 || PASS=0

if [[ $PASS -eq 1 ]]; then VERDICT=PASS; else VERDICT=FAIL; fi
cat > "$RECORD_DIR/SMOKE_REPORT.md" <<REPORT
# stage10_pci smoke report
- interface: $IFNAME
- proto: $PROTO
- count: $COUNT
- record_dir: $RECORD_DIR
- verdict: $VERDICT
REPORT

echo "[stage10_pci] smoke artifacts -> $RECORD_DIR ($VERDICT)"
echo "=== queue_dist ==="
cat "$RECORD_DIR/queue_dist_check.txt"
echo "=== vector_check ==="
cat "$RECORD_DIR/vector_check.txt"
echo "=== timeline ==="
cat "$RECORD_DIR/timeline_check.txt"
echo "=== irq ==="
cat "$RECORD_DIR/irq_check.txt"
[[ $PASS -eq 1 ]]
