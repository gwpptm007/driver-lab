#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$DIR/common.sh"

BASE_DIR=${1:-records}
NAME=${2:-queue-poll-observe}
OUT_DIR="$BASE_DIR/$(ts_now)-$NAME"
mkdir -p "$OUT_DIR"

cp -f records/templates/SUMMARY_TEMPLATE.md "$OUT_DIR/SUMMARY.md"
cp -f records/templates/CHAIN_REVIEW_NOTE_TEMPLATE.md "$OUT_DIR/CHAIN_REVIEW_NOTE.md"
cp -f records/templates/COMPARE_TEMPLATE.md "$OUT_DIR/IDLE_PING_IPERF_COMPARE.md"
cp -f records/templates/WINDOW_NOTE_TEMPLATE.md "$OUT_DIR/window_note.md"

echo "$OUT_DIR"
