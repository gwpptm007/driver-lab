#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$ROOT_DIR/tools"
SCRIPTS_DIR="$ROOT_DIR/scripts"

IFNAME=${IFNAME:-nds8}
ETHERTYPE=${ETHERTYPE:-0x88B8}
SMOKE_COUNT=${SMOKE_COUNT:-32}
SMOKE_TIMEOUT_SEC=${SMOKE_TIMEOUT_SEC:-5}
RECV_READY_SLEEP_SEC=${RECV_READY_SLEEP_SEC:-1}
SEND_PAYLOAD=${SEND_PAYLOAD:-hello_stage08_async_backend}

STAMP=$(date +%Y%m%d-%H%M%S)
LOG_DIR="$ROOT_DIR/records/$STAMP-stage08-smoke"
SEND_TOOL="$TOOLS_DIR/send_stage08_frame"
RECV_TOOL="$TOOLS_DIR/recv_stage08_frame"
DBG_DIR=/sys/kernel/debug/netdev_stage08

mkdir -p "$LOG_DIR"

test -x "$SEND_TOOL" || { echo "[stage08] missing $SEND_TOOL, run scripts/build.sh first" >&2; exit 1; }
test -x "$RECV_TOOL" || { echo "[stage08] missing $RECV_TOOL, run scripts/build.sh first" >&2; exit 1; }
lsmod | awk '{print $1}' | grep -qx netdev_stage08 || { echo "[stage08] module netdev_stage08 is not loaded" >&2; exit 1; }

sudo -n true >/dev/null 2>&1 || {
    echo "[stage08] sudo -n true failed. 请先配置免密 sudo，或先执行一次 sudo true 缓存凭证。" >&2
    exit 1
}

collect_snapshot() {
    local phase=$1
    ip -s link show "$IFNAME" | tee "$LOG_DIR/ip_link_${phase}.txt" >/dev/null || true
    [[ -f "$DBG_DIR/stats" ]] && sudo cat "$DBG_DIR/stats" > "$LOG_DIR/debugfs_stats_${phase}.txt"
    [[ -f "$DBG_DIR/queues" ]] && sudo cat "$DBG_DIR/queues" > "$LOG_DIR/debugfs_queues_${phase}.txt"
    [[ -f "$DBG_DIR/timeline" ]] && sudo cat "$DBG_DIR/timeline" > "$LOG_DIR/debugfs_timeline_${phase}.txt"
    [[ -f "$DBG_DIR/test_stats" ]] && sudo cat "$DBG_DIR/test_stats" > "$LOG_DIR/debugfs_test_stats_${phase}.txt"
}

sudo ip link set dev "$IFNAME" up
collect_snapshot before

RECV_RC=0
STATS_RC=0
TIMELINE_RC=0
VERDICT=FAIL
FAIL_REASONS=()

sudo "$RECV_TOOL" "$IFNAME" "$ETHERTYPE" "$SMOKE_COUNT" "$SMOKE_TIMEOUT_SEC" \
    > "$LOG_DIR/recv.txt" 2>&1 &
REC_PID=$!

sleep "$RECV_READY_SLEEP_SEC"

if ! sudo "$SEND_TOOL" "$IFNAME" "$SEND_PAYLOAD" "$ETHERTYPE" "$SMOKE_COUNT" 0 \
    | tee "$LOG_DIR/send.txt"; then
    FAIL_REASONS+=("sender_failed")
fi

if wait "$REC_PID"; then
    RECV_RC=0
else
    RECV_RC=$?
    FAIL_REASONS+=("receiver_exit_${RECV_RC}")
fi

sleep 1
collect_snapshot after
sudo dmesg | tail -n 160 > "$LOG_DIR/dmesg_tail.txt" || true

if "$SCRIPTS_DIR/stats_check.sh" "$LOG_DIR" "$SMOKE_COUNT" > "$LOG_DIR/stats_check.txt" 2>&1; then
    STATS_RC=0
else
    STATS_RC=$?
    FAIL_REASONS+=("stats_check_${STATS_RC}")
fi

if "$SCRIPTS_DIR/timeline_check.sh" "$LOG_DIR" > "$LOG_DIR/timeline_check.txt" 2>&1; then
    TIMELINE_RC=0
else
    TIMELINE_RC=$?
    FAIL_REASONS+=("timeline_check_${TIMELINE_RC}")
fi

if ! grep -q "sent ${SMOKE_COUNT} frame(s)" "$LOG_DIR/send.txt"; then
    FAIL_REASONS+=("send_summary_missing")
fi
if [[ ! -s "$LOG_DIR/recv.txt" ]]; then
    FAIL_REASONS+=("recv_empty")
fi
if ! grep -q "received .*matched_magic=${SMOKE_COUNT}" "$LOG_DIR/recv.txt"; then
    FAIL_REASONS+=("recv_summary_missing_or_incomplete")
fi
if ! grep -q "proto=$(printf '0x%04x' $((ETHERTYPE)))" "$LOG_DIR/recv.txt" 2>/dev/null; then
    # grep 的数值格式不稳定时退化为检查 0x88b8/0x88B8
    if ! grep -Eqi "proto=0x88b8|proto=0x88B8" "$LOG_DIR/recv.txt"; then
        FAIL_REASONS+=("recv_proto_missing")
    fi
fi

if [[ ${#FAIL_REASONS[@]} -eq 0 && $RECV_RC -eq 0 && $STATS_RC -eq 0 && $TIMELINE_RC -eq 0 ]]; then
    VERDICT=PASS
fi

cat > "$LOG_DIR/SMOKE_REPORT.md" <<EOF
# stage08 smoke report

- ifname: $IFNAME
- ethertype: $ETHERTYPE
- count: $SMOKE_COUNT
- timeout_sec: $SMOKE_TIMEOUT_SEC
- receiver_rc: $RECV_RC
- stats_check_rc: $STATS_RC
- timeline_check_rc: $TIMELINE_RC
- verdict: $VERDICT

## fail_reasons

$(printf -- '- %s\n' "${FAIL_REASONS[@]:-none}")

## artifacts

- send.txt
- recv.txt
- debugfs_stats_before.txt / debugfs_stats_after.txt
- debugfs_test_stats_before.txt / debugfs_test_stats_after.txt （若内核支持）
- debugfs_timeline_before.txt / debugfs_timeline_after.txt
- debugfs_queues_before.txt / debugfs_queues_after.txt
- ip_link_before.txt / ip_link_after.txt
- stats_check.txt
- timeline_check.txt
- dmesg_tail.txt
EOF

echo "[stage08] smoke record -> $LOG_DIR"
echo "[stage08] verdict -> $VERDICT"
[[ "$VERDICT" == PASS ]]
