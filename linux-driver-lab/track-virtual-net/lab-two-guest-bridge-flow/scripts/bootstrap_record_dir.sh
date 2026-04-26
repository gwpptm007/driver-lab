#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
source "$DIR/common.sh"

BASE=${1:-records}
OUT="$BASE/$(ts_now)-two-guest-bridge-flow"
mkdir -p "$OUT"

cp -f records/templates/SUMMARY_TEMPLATE.md "$OUT/SUMMARY.md"
cp -f records/templates/TOPOLOGY_NOTE_TEMPLATE.md "$OUT/TOPOLOGY_NOTE.md"
cp -f records/templates/FLOW_REVIEW_NOTE_TEMPLATE.md "$OUT/FLOW_REVIEW_NOTE.md"

echo "$OUT"
