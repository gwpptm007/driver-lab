#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:-eth0}
OUT_DIR=${2:-records/manual-collect}

mkdir -p "$OUT_DIR"
ip -s link show dev "$IFNAME" > "$OUT_DIR/ip_link.txt" || true
ethtool -S "$IFNAME" > "$OUT_DIR/ethtool_S.txt" || true
ethtool -k "$IFNAME" > "$OUT_DIR/ethtool_k.txt" || true
dmesg > "$OUT_DIR/dmesg.txt" || true
uname -a > "$OUT_DIR/uname.txt" || true
cp docs/06_REPORT_TEMPLATE.md "$OUT_DIR/SUMMARY.md" || true
echo "$OUT_DIR"
