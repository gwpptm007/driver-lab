#!/usr/bin/env bash
set -euo pipefail
OUT_DIR=${1:?usage: $0 <record-dir>}

for mode in idle ping iperf; do
    before="$OUT_DIR/${mode}_before_ethtool_S.txt"
    after="$OUT_DIR/${mode}_after_ethtool_S.txt"
    if [[ -f "$before" && -f "$after" ]]; then
        diff -u "$before" "$after" > "$OUT_DIR/${mode}_ethtool_S.diff" || true
    fi
done

echo "delta summary files written under: $OUT_DIR"
