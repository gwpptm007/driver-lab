#!/usr/bin/env bash
set -euo pipefail
OUT_FILE=${1:?usage: $0 <out-file>}
dmesg | tail -n 200 > "$OUT_FILE" 2>&1 || true
echo "$OUT_FILE"
