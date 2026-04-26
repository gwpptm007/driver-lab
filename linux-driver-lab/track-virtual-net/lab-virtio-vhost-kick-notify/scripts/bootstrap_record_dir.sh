#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$DIR/common.sh"

BASE=${1:-records}
OUT="$BASE/$(ts_now)-virtio-vhost-kick-notify"
mkdir -p "$OUT/off" "$OUT/on" "$OUT/before"

cp -f records/templates/SUMMARY_TEMPLATE.md "$OUT/SUMMARY.md"
cp -f records/templates/VHOST_COMPARE_NOTE_TEMPLATE.md "$OUT/VHOST_COMPARE_NOTE.md"
cp -f records/templates/KICK_NOTIFY_NOTE_TEMPLATE.md "$OUT/KICK_NOTIFY_NOTE.md"
cp -f records/templates/MODE_RESULT_TEMPLATE.md "$OUT/off/MODE_RESULT.md"
cp -f records/templates/MODE_RESULT_TEMPLATE.md "$OUT/on/MODE_RESULT.md"

echo "$OUT"
