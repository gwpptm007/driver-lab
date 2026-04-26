#!/usr/bin/env bash
set -euo pipefail
OUT_DIR=${1:?usage: $0 <record-dir>}

BEFORE="$OUT_DIR/ethtool_S_before.txt"
AFTER="$OUT_DIR/ethtool_S_after.txt"
DIFF_OUT="$OUT_DIR/ethtool_S.diff"

if [[ ! -f "$BEFORE" || ! -f "$AFTER" ]]; then
    echo "missing before/after stats files under $OUT_DIR" >&2
    exit 1
fi

diff -u "$BEFORE" "$AFTER" > "$DIFF_OUT" || true
echo "$DIFF_OUT"
