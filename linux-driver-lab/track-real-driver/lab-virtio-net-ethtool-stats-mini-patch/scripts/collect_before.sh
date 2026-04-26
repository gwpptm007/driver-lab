#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <record-dir>}
OUT_DIR=${2:?usage: $0 <ifname> <record-dir>}

mkdir -p "$OUT_DIR"

ethtool -i "$IFNAME" > "$OUT_DIR/ethtool_i_before.txt" 2>&1 || true
ethtool -S "$IFNAME" > "$OUT_DIR/ethtool_S_before.txt" 2>&1 || true
ip -s link show "$IFNAME" > "$OUT_DIR/ip_link_before.txt" 2>&1 || true
uname -a > "$OUT_DIR/uname_before.txt" 2>&1 || true
dmesg | tail -n 200 > "$OUT_DIR/dmesg_before_tail.txt" 2>&1 || true

echo "before collected: $OUT_DIR"
