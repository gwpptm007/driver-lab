#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <record-dir>}
OUT_DIR=${2:?usage: $0 <ifname> <record-dir>}

mkdir -p "$OUT_DIR"

ethtool -i "$IFNAME" > "$OUT_DIR/ethtool_i_after.txt" 2>&1 || true
ethtool -S "$IFNAME" > "$OUT_DIR/ethtool_S_after.txt" 2>&1 || true
ip -s link show "$IFNAME" > "$OUT_DIR/ip_link_after.txt" 2>&1 || true
dmesg | tail -n 200 > "$OUT_DIR/dmesg_after_tail.txt" 2>&1 || true

echo "after collected: $OUT_DIR"
