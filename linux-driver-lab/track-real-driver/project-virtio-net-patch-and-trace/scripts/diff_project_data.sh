#!/usr/bin/env bash
set -euo pipefail
OUT_DIR=${1:?usage: $0 <record-dir>}
mkdir -p "$OUT_DIR/after"

if [[ -f "$OUT_DIR/before/ethtool_S.txt" && -f "$OUT_DIR/after/ethtool_S.txt" ]]; then
    diff -u "$OUT_DIR/before/ethtool_S.txt" "$OUT_DIR/after/ethtool_S.txt" > "$OUT_DIR/after/ethtool_S.diff" || true
fi

if [[ -f "$OUT_DIR/before/ip_link.txt" && -f "$OUT_DIR/after/ip_link.txt" ]]; then
    diff -u "$OUT_DIR/before/ip_link.txt" "$OUT_DIR/after/ip_link.txt" > "$OUT_DIR/after/ip_link.diff" || true
fi

echo "project diff files written under: $OUT_DIR/after"
