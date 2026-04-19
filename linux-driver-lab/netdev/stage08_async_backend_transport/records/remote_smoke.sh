#!/bin/bash
IFNAME=nds8
TOOLS=/home/wq7/workspace/driver-lab/linux-driver-lab/netdev/stage08_async_backend_transport/tools
DBG=/sys/kernel/debug/netdev_stage08
STAMP=$(date +%Y%m%d-%H%M%S)
LOG=/home/wq7/workspace/driver-lab/linux-driver-lab/netdev/stage08_async_backend_transport/records/${STAMP}-direct
mkdir -p "$LOG"

echo "[$(date)] Starting smoke test" | tee "$LOG/log.txt"

# Get stats before
cat "$DBG/stats" > "$LOG/stats_before.txt"

# Start recv in background
"$TOOLS/recv_stage08_frame" "$IFNAME" 0x88B8 32 5 > "$LOG/recv.txt" 2>&1 &
RPID=$!

# Give recv time to bind
sleep 1

# Send
echo "[$(date)] Sending 32 frames" | tee -a "$LOG/log.txt"
"$TOOLS/send_stage08_frame" "$IFNAME" direct_test 0x88B8 32 0 | tee -a "$LOG/log.txt"

# Wait for recv
wait $RPID
RRC=$?
echo "[$(date)] recv exit=$RRC" | tee -a "$LOG/log.txt"

# Get stats after
cat "$DBG/stats" > "$LOG/stats_after.txt"
cat "$DBG/timeline" > "$LOG/timeline_after.txt"

# Show results
echo "=== RECV ==="
cat "$LOG/recv.txt"
echo "=== SEND ==="
grep "sent" "$LOG/log.txt"
echo "=== STATS DIFF ==="
diff "$LOG/stats_before.txt" "$LOG/stats_after.txt" || true
echo "Log: $LOG"