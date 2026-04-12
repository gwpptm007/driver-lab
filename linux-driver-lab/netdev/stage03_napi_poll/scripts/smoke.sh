#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
IFNAME=${IFNAME:-nds3}
ETHERTYPE=${ETHERTYPE:-0x88B6}
SMOKE_COUNT=${SMOKE_COUNT:-16}
SMOKE_TIMEOUT_SEC=${SMOKE_TIMEOUT_SEC:-5}
TIMEOUT_BIN=${TIMEOUT_BIN:-timeout}
SEND_TOOL="$ROOT_DIR/tools/send_stage03_frame"
RECV_TOOL="$ROOT_DIR/tools/recv_stage03_frame"
RX_MODE=${RX_MODE:-napi}
STAMP=$(date +%Y%m%d-%H%M%S)
LOG_DIR="$ROOT_DIR/records/$STAMP-stage03-smoke-$RX_MODE"
mkdir -p "$LOG_DIR"

if [[ ! -x "$SEND_TOOL" || ! -x "$RECV_TOOL" ]]; then
    echo "[stage03] userspace tools missing, run 'make build-userspace' first" >&2
    exit 1
fi

if ! lsmod | awk '{print $1}' | grep -qx netdev_stage03; then
    echo "[stage03] module netdev_stage03 is not loaded" >&2
    exit 1
fi

sudo ip link set dev "$IFNAME" up
ip -details link show "$IFNAME" | tee "$LOG_DIR/ip_link_before.txt"

sudo "$TIMEOUT_BIN" "$SMOKE_TIMEOUT_SEC" \
    "$RECV_TOOL" "$IFNAME" "$ETHERTYPE" "$SMOKE_COUNT" "$SMOKE_TIMEOUT_SEC" \
    > "$LOG_DIR/recv.txt" 2>&1 &
RECV_PID=$!
sleep 1

sudo "$SEND_TOOL" "$IFNAME" "hello_stage03_burst" "$ETHERTYPE" "$SMOKE_COUNT" 0 \
    | tee "$LOG_DIR/send_tool.txt"
wait "$RECV_PID" || true
sleep 1

ip -s link show "$IFNAME" | tee "$LOG_DIR/ip_link_after.txt"
if [[ -f /sys/kernel/debug/netdev_stage03/stats ]]; then
    sudo cat /sys/kernel/debug/netdev_stage03/stats | tee "$LOG_DIR/debugfs_stats.txt"
fi

echo "[stage03] smoke record -> $LOG_DIR"
