#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <peer-ip> <record-dir> [count]}
PEER_IP=${2:?usage: $0 <ifname> <peer-ip> <record-dir> [count]}
OUT_DIR=${3:?usage: $0 <ifname> <peer-ip> <record-dir> [count]}
COUNT=${4:-20}
DIR=$(cd -- "$(dirname -- "$0")" && pwd)

mkdir -p "$OUT_DIR"
"$DIR/collect_stats_snapshot.sh" "$IFNAME" "$OUT_DIR/ping_before"
ping -I "$IFNAME" -c "$COUNT" "$PEER_IP" > "$OUT_DIR/ping_output.txt" 2>&1 || true
"$DIR/collect_stats_snapshot.sh" "$IFNAME" "$OUT_DIR/ping_after"
echo "ping window complete: $OUT_DIR"
