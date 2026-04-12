#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
IFNAME=${IFNAME:-nds0}
USER_TOOL="$ROOT_DIR/tools/send_stage01_frame"
STAMP=$(date +%Y%m%d-%H%M%S)
LOG_DIR="$ROOT_DIR/records/$STAMP-stage01-smoke"
mkdir -p "$LOG_DIR"

if [[ ! -x "$USER_TOOL" ]]; then
    echo "[stage01] userspace tool missing, run 'make build-userspace' first" >&2
    exit 1
fi

if ! lsmod | awk '{print $1}' | grep -qx netdev_stage01; then
    echo "[stage01] module netdev_stage01 is not loaded" >&2
    exit 1
fi

sudo ip link set dev "$IFNAME" up
ip -details link show "$IFNAME" | tee "$LOG_DIR/ip_link_before.txt"

sudo "$USER_TOOL" "$IFNAME" "hello_stage01" | tee "$LOG_DIR/send_tool.txt"
sleep 1

ip -s link show "$IFNAME" | tee "$LOG_DIR/ip_link_after.txt"
if [[ -f /sys/kernel/debug/netdev_stage01/stats ]]; then
    sudo cat /sys/kernel/debug/netdev_stage01/stats | tee "$LOG_DIR/debugfs_stats.txt"
fi

echo "[stage01] smoke record -> $LOG_DIR"
