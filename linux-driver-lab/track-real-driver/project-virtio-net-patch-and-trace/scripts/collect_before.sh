#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <record-dir>}
OUT_DIR=${2:?usage: $0 <ifname> <record-dir>}
mkdir -p "$OUT_DIR/before"

ethtool -i "$IFNAME" > "$OUT_DIR/before/ethtool_i.txt" 2>&1 || true
ethtool -S "$IFNAME" > "$OUT_DIR/before/ethtool_S.txt" 2>&1 || true
ip -s link show "$IFNAME" > "$OUT_DIR/before/ip_link.txt" 2>&1 || true
uname -a > "$OUT_DIR/before/uname.txt" 2>&1 || true
dmesg | tail -n 200 > "$OUT_DIR/before/dmesg_tail.txt" 2>&1 || true

echo "before collected under: $OUT_DIR/before"
