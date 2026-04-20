#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# smoke.sh — stage13_soft 完整 smoke test
#
# 测试内容：
#   1. 发送 64 帧测试流量
#   2. 验证多队列分发（queue_dist）
#   3. 验证 MSI-X vector 处理（vector_check）
#   4. 验证异步链路 timeline（timeline_check）
#   5. 验证 page_pool 分配（page_pool check）

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
STAGE13_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
TOOLS_DIR="$STAGE13_DIR/tools"
IFNAME=${IFNAME:-nds13s}
COUNT=${COUNT:-64}
RECORD_DIR="$STAGE13_DIR/records/$(date +%Y%m%d-%H%M%S)-stage13-soft-smoke"
DBG_DIR=${DBG_DIR:-/sys/kernel/debug/netdev_stage13_soft}

mkdir -p "$RECORD_DIR"

sudo -n true >/dev/null 2>&1 || {
    echo "[stage13_soft] sudo needed but -n failed; please pre-authorize sudo" >&2
    exit 1
}

if [[ ! -x "$TOOLS_DIR/send_stage13_frame" || ! -x "$TOOLS_DIR/recv_stage13_frame" ]]; then
    echo "[stage13_soft] tools missing, run scripts/build.sh first" >&2
    exit 1
fi

capture_debugfs() {
    local suffix=$1
    if [[ -d "$DBG_DIR" ]]; then
        sudo cat "$DBG_DIR/stats" > "$RECORD_DIR/debugfs_stats_${suffix}.txt" 2>/dev/null || true
        sudo cat "$DBG_DIR/queues" > "$RECORD_DIR/debugfs_queues_${suffix}.txt" 2>/dev/null || true
        sudo cat "$DBG_DIR/timeline" > "$RECORD_DIR/debugfs_timeline_${suffix}.txt" 2>/dev/null || true
        sudo cat "$DBG_DIR/vectors" > "$RECORD_DIR/debugfs_vectors_${suffix}.txt" 2>/dev/null || true
        sudo cat "$DBG_DIR/page_pool" > "$RECORD_DIR/debugfs_page_pool_${suffix}.txt" 2>/dev/null || true
        sudo cat "$DBG_DIR/offload" > "$RECORD_DIR/debugfs_offload_${suffix}.txt" 2>/dev/null || true
    fi
}

echo "[stage13_soft] === capture before ==="
capture_debugfs before
ip -s link show dev "$IFNAME" > "$RECORD_DIR/ip_link_before.txt" 2>/dev/null || true

echo "[stage13_soft] === send $COUNT frames ==="
"$TOOLS_DIR/recv_stage13_frame" --ifname "$IFNAME" --timeout-sec 5 --count 1 > "$RECORD_DIR/recv.txt" 2>&1 &
RECV_PID=$!
sleep 0.5
PASS=1
"$TOOLS_DIR/send_stage13_frame" --ifname "$IFNAME" --count "$COUNT" > "$RECORD_DIR/send.txt" 2>&1 || PASS=0
wait "$RECV_PID" || PASS=0

echo "[stage13_soft] === capture after ==="
capture_debugfs after
ip -s link show dev "$IFNAME" > "$RECORD_DIR/ip_link_after.txt" 2>/dev/null || true
dmesg | tail -n 200 > "$RECORD_DIR/dmesg_tail.txt" 2>/dev/null || true

"$SCRIPT_DIR/queue_dist_check.sh" "$RECORD_DIR" > "$RECORD_DIR/queue_dist_check.txt" 2>&1 || PASS=0
"$SCRIPT_DIR/vector_check.sh" "$RECORD_DIR" > "$RECORD_DIR/vector_check.txt" 2>&1 || PASS=0
"$SCRIPT_DIR/timeline_check.sh" "$RECORD_DIR" > "$RECORD_DIR/timeline_check.txt" 2>&1 || PASS=0
"$SCRIPT_DIR/pp_check.sh" "$RECORD_DIR" > "$RECORD_DIR/pp_check.txt" 2>&1 || PASS=0

if [[ $PASS -eq 1 ]]; then VERDICT=PASS; else VERDICT=FAIL; fi
cat > "$RECORD_DIR/SMOKE_REPORT.md" <<REPORT
# stage13_soft smoke report
- interface: $IFNAME
- count: $COUNT
- frame_type: ETH_P_IP (0x0800) + IP-proto=253
- record_dir: $RECORD_DIR
- verdict: $VERDICT
REPORT

echo "[stage13_soft] smoke artifacts -> $RECORD_DIR ($VERDICT)"
echo "=== queue_dist ==="
cat "$RECORD_DIR/queue_dist_check.txt"
echo "=== vector_check ==="
cat "$RECORD_DIR/vector_check.txt"
echo "=== timeline ==="
cat "$RECORD_DIR/timeline_check.txt"
echo "=== page_pool ==="
cat "$RECORD_DIR/pp_check.txt"
[[ $PASS -eq 1 ]]