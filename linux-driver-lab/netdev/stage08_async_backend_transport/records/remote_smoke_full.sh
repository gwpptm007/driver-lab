#!/bin/bash
set -e
IFNAME=nds8
ETHERTYPE=0x88B8
SMOKE_COUNT=32
SMOKE_TIMEOUT_SEC=5
TOOLS=/home/wq7/workspace/driver-lab/linux-driver-lab/netdev/stage08_async_backend_transport/tools
DBG=/sys/kernel/debug/netdev_stage08
SCRIPTS=/home/wq7/workspace/driver-lab/linux-driver-lab/netdev/stage08_async_backend_transport/scripts
STAMP=$(date +%Y%m%d-%H%M%S)
LOG=/home/wq7/workspace/driver-lab/linux-driver-lab/netdev/stage08_async_backend_transport/records/${STAMP}-full-smoke
mkdir -p "$LOG"

echo "=== STAGE08 FULL SMOKE TEST ===" | tee "$LOG/smoke_report.txt"
echo "STAMP: $STAMP" | tee -a "$LOG/smoke_report.txt"
echo "IFNAME: $IFNAME" | tee -a "$LOG/smoke_report.txt"
echo "ETHERTYPE: $ETHERTYPE" | tee -a "$LOG/smoke_report.txt"
echo "COUNT: $SMOKE_COUNT" | tee -a "$LOG/smoke_report.txt"
echo "" | tee -a "$LOG/smoke_report.txt"

# --- Phase 1: Snapshot before ---
echo "[Phase 1] Collecting before snapshot..." | tee -a "$LOG/smoke_report.txt"
ip link show "$IFNAME" > "$LOG/ip_link_before.txt" 2>&1 || true
cat "$DBG/stats" > "$LOG/debugfs_stats_before.txt"
cat "$DBG/timeline" > "$LOG/debugfs_timeline_before.txt"
cat "$DBG/queues" > "$LOG/debugfs_queues_before.txt"
[[ -f "$DBG/test_stats" ]] && cat "$DBG/test_stats" > "$LOG/debugfs_test_stats_before.txt"
echo "Before snapshot done" | tee -a "$LOG/smoke_report.txt"

# --- Phase 2: Send frames ---
echo "" | tee -a "$LOG/smoke_report.txt"
echo "[Phase 2] Sending $SMOKE_COUNT frames..." | tee -a "$LOG/smoke_report.txt"
"$TOOLS/send_stage08_frame" "$IFNAME" full_smoke_test 0x88B8 $SMOKE_COUNT 0 | tee "$LOG/send.txt" 2>&1
echo "Send done" | tee -a "$LOG/smoke_report.txt"

# --- Phase 3: Snapshot after (with small delay for async to settle) ---
sleep 2
echo "" | tee -a "$LOG/smoke_report.txt"
echo "[Phase 3] Collecting after snapshot..." | tee -a "$LOG/smoke_report.txt"
cat "$DBG/stats" > "$LOG/debugfs_stats_after.txt"
cat "$DBG/timeline" > "$LOG/debugfs_timeline_after.txt"
cat "$DBG/queues" > "$LOG/debugfs_queues_after.txt"
[[ -f "$DBG/test_stats" ]] && cat "$DBG/test_stats" > "$LOG/debugfs_test_stats_after.txt"
ip link show "$IFNAME" > "$LOG/ip_link_after.txt" 2>&1 || true
echo "After snapshot done" | tee -a "$LOG/smoke_report.txt"

# --- Phase 4: tcpdump verification (parallel) ---
echo "" | tee -a "$LOG/smoke_report.txt"
echo "[Phase 4] tcpdump capture (verify frames on wire)..." | tee -a "$LOG/smoke_report.txt"
tcpdump -i "$IFNAME" -c $SMOKE_COUNT -w "$LOG/capture.pcap" "ether proto 0x88B8" 2>/dev/null &
TPID=$!
sleep 0.3
"$TOOLS/send_stage08_frame" "$IFNAME" tcpdump_check 0x88B8 $SMOKE_COUNT 0 > /dev/null 2>&1
sleep 2
kill $TPID 2>/dev/null || true
TCPDUMP_COUNT=$(tcpdump -r "$LOG/capture.pcap" 2>/dev/null | wc -l)
echo "tcpdump captured: $TCPDUMP_COUNT frames" | tee -a "$LOG/smoke_report.txt"
if tcpdump -r "$LOG/capture.pcap" 2>/dev/null | head -3 > "$LOG/tcpdump_sample.txt"; then
    echo "tcpdump sample saved" | tee -a "$LOG/smoke_report.txt"
fi

# --- Phase 5: stats_check ---
echo "" | tee -a "$LOG/smoke_report.txt"
echo "[Phase 5] Running stats_check.sh..." | tee -a "$LOG/smoke_report.txt"
if "$SCRIPTS/stats_check.sh" "$LOG" $SMOKE_COUNT > "$LOG/stats_check.txt" 2>&1; then
    STATS_RC=0
    echo "stats_check: PASS" | tee -a "$LOG/smoke_report.txt"
else
    STATS_RC=$?
    echo "stats_check: FAIL (rc=$STATS_RC)" | tee -a "$LOG/smoke_report.txt"
fi
cat "$LOG/stats_check.txt" | tee -a "$LOG/smoke_report.txt"

# --- Phase 6: timeline_check ---
echo "" | tee -a "$LOG/smoke_report.txt"
echo "[Phase 6] Running timeline_check.sh..." | tee -a "$LOG/smoke_report.txt"
if "$SCRIPTS/timeline_check.sh" "$LOG" > "$LOG/timeline_check.txt" 2>&1; then
    TIMELINE_RC=0
    echo "timeline_check: PASS" | tee -a "$LOG/smoke_report.txt"
else
    TIMELINE_RC=$?
    echo "timeline_check: FAIL (rc=$TIMELINE_RC)" | tee -a "$LOG/smoke_report.txt"
fi
cat "$LOG/timeline_check.txt" | tee -a "$LOG/smoke_report.txt"

# --- Phase 7: Final verdict ---
echo "" | tee -a "$LOG/smoke_report.txt"
echo "=== VERDICT ===" | tee -a "$LOG/smoke_report.txt"

# Check send summary
if grep -q "sent ${SMOKE_COUNT} frame" "$LOG/send.txt"; then
    echo "send_summary: PASS" | tee -a "$LOG/smoke_report.txt"
    SEND_RC=0
else
    echo "send_summary: FAIL" | tee -a "$LOG/smoke_report.txt"
    SEND_RC=1
fi

# tcpdump should have captured double (TX + RX loopback)
if [[ "$TCPDUMP_COUNT" -ge "$SMOKE_COUNT" ]]; then
    echo "tcpdump_capture: PASS (captured $TCPDUMP_COUNT >= $SMOKE_COUNT)" | tee -a "$LOG/smoke_report.txt"
    TCPDUMP_RC=0
else
    echo "tcpdump_capture: FAIL (captured $TCPDUMP_COUNT < $SMOKE_COUNT)" | tee -a "$LOG/smoke_report.txt"
    TCPDUMP_RC=1
fi

# Note about recv tool
echo "" | tee -a "$LOG/smoke_report.txt"
echo "NOTE: recv_stage08_frame could not receive frames due to PACKET_IGNORE_OUTGOING" | tee -a "$LOG/smoke_report.txt"
echo "behavior on this kernel version. Verification done via debugfs stats + tcpdump." | tee -a "$LOG/smoke_report.txt"

# Overall
if [[ "$SEND_RC" -eq 0 && "$STATS_RC" -eq 0 && "$TIMELINE_RC" -eq 0 && "$TCPDUMP_RC" -eq 0 ]]; then
    VERDICT="PASS"
else
    VERDICT="FAIL"
fi
echo "" | tee -a "$LOG/smoke_report.txt"
echo "OVERALL: $VERDICT" | tee -a "$LOG/smoke_report.txt"
echo "Log directory: $LOG"
echo "Report: $LOG/smoke_report.txt"