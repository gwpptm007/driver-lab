#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <record-dir> [seconds]}
OUT_DIR=${2:?usage: $0 <ifname> <record-dir> [seconds]}
SECONDS=${3:-10}
DIR=$(cd -- "$(dirname -- "$0")" && pwd)

mkdir -p "$OUT_DIR"
"$DIR/collect_stats_snapshot.sh" "$IFNAME" "$OUT_DIR/idle_before"
sleep "$SECONDS"
"$DIR/collect_stats_snapshot.sh" "$IFNAME" "$OUT_DIR/idle_after"
echo "idle window complete: $OUT_DIR"
