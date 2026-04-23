#!/usr/bin/env bash
set -euo pipefail
ROUND_NAME=${1:-round-manual}
BASE_DIR=${2:-records}
TS=$(date +%Y%m%d_%H%M%S)
OUT_DIR="$BASE_DIR/${TS}-${ROUND_NAME}"
mkdir -p "$OUT_DIR"
cp -f records/templates/ROUND_SUMMARY_TEMPLATE.md "$OUT_DIR/SUMMARY.md"
cp -f records/templates/FUNCTION_NOTE_TEMPLATE.md "$OUT_DIR/FUNCTION_NOTE_TEMPLATE.md"
cp -f records/templates/TRACE_NOTE_TEMPLATE.md "$OUT_DIR/TRACE_NOTE_TEMPLATE.md"
echo "$OUT_DIR"
