#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
source "$DIR/common.sh"

BASE=${1:-records}
OUT="$BASE/$(ts_now)-virtual-net-end-to-end"
mkdir -p "$OUT"

cp -f records/templates/FINAL_PROJECT_REPORT_TEMPLATE.md "$OUT/FINAL_PROJECT_REPORT.md"
cp -f records/templates/EVIDENCE_INDEX_TEMPLATE.md "$OUT/EVIDENCE_INDEX.md"
cp -f records/templates/SHARE_SCRIPT_TEMPLATE.md "$OUT/SHARE_SCRIPT.md"

echo "$OUT"
