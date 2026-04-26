#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$DIR/common.sh"

BASE_DIR=${1:-records}
NAME=${2:-e1000e-source-compare}
OUT_DIR="$BASE_DIR/$(ts_now)-$NAME"
mkdir -p "$OUT_DIR"

cp -f records/templates/SUMMARY_TEMPLATE.md "$OUT_DIR/SUMMARY.md"
cp -f records/templates/FUNCTION_NOTE_TEMPLATE.md "$OUT_DIR/FUNCTION_NOTE.md"
cp -f records/templates/COMPARE_NOTE_TEMPLATE.md "$OUT_DIR/COMPARE_NOTE.md"

echo "$OUT_DIR"
