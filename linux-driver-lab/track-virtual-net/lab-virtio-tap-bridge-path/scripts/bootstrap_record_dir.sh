#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
source "$DIR/common.sh"

BASE=${1:-records}
OUT="$BASE/$(ts_now)-virtio-tap-bridge-path"
mkdir -p "$OUT"
cp -f records/templates/SUMMARY_TEMPLATE.md "$OUT/SUMMARY.md"
cp -f records/templates/TOPOLOGY_NOTE_TEMPLATE.md "$OUT/TOPOLOGY_NOTE.md"
echo "$OUT"
