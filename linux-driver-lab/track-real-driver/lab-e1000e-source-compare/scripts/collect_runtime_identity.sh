#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <out-dir>}
OUT_DIR=${2:?usage: $0 <ifname> <out-dir>}
mkdir -p "$OUT_DIR"

ethtool -i "$IFNAME" > "$OUT_DIR/ethtool_i.txt" 2>&1 || true
ethtool -S "$IFNAME" > "$OUT_DIR/ethtool_S.txt" 2>&1 || true
ip -s link show "$IFNAME" > "$OUT_DIR/ip_link.txt" 2>&1 || true
lspci -nnk > "$OUT_DIR/lspci_nnk.txt" 2>&1 || true

echo "runtime identity collected under: $OUT_DIR"
