#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$DIR/common.sh"

BASE_DIR=${1:-records}
NAME=${2:-virtio-net-patch-trace}
OUT_DIR="$BASE_DIR/$(ts_now)-$NAME"
mkdir -p "$OUT_DIR/before" "$OUT_DIR/after" "$OUT_DIR/trace"

cp -f records/templates/SUMMARY_TEMPLATE.md "$OUT_DIR/SUMMARY.md"
cp -f records/templates/PATCH_POINT_NOTE_TEMPLATE.md "$OUT_DIR/PATCH_POINT_NOTE.md"
cp -f records/templates/PATCH_REVIEW_NOTE_TEMPLATE.md "$OUT_DIR/PATCH_REVIEW_NOTE.md"
cp -f records/templates/TRACE_REVIEW_NOTE_TEMPLATE.md "$OUT_DIR/TRACE_REVIEW_NOTE.md"
cp -f records/templates/FINAL_PROJECT_NOTE_TEMPLATE.md "$OUT_DIR/FINAL_PROJECT_NOTE.md"
cp -f records/templates/RISK_NOTE_TEMPLATE.md "$OUT_DIR/RISK_NOTE.md"

echo "$OUT_DIR"
