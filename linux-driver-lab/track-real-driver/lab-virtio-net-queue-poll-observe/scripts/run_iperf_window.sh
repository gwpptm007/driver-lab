#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <server-ip> <record-dir> [seconds]}
SERVER_IP=${2:?usage: $0 <ifname> <server-ip> <record-dir> [seconds]}
OUT_DIR=${3:?usage: $0 <ifname> <server-ip> <record-dir> [seconds]}
SECONDS=${4:-10}
DIR=$(cd -- "$(dirname -- "$0")" && pwd)

mkdir -p "$OUT_DIR"
"$DIR/collect_stats_snapshot.sh" "$IFNAME" "$OUT_DIR/iperf_before"
iperf3 -B "$IFNAME" -c "$SERVER_IP" -t "$SECONDS" > "$OUT_DIR/iperf_output.txt" 2>&1 || true
"$DIR/collect_stats_snapshot.sh" "$IFNAME" "$OUT_DIR/iperf_after"
echo "iperf window complete: $OUT_DIR"
