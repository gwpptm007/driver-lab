#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <server-ip> <record-dir> [seconds]}
SERVER_IP=${2:?usage: $0 <ifname> <server-ip> <record-dir> [seconds]}
OUT_DIR=${3:?usage: $0 <ifname> <server-ip> <record-dir> [seconds]}
SECONDS=${4:-10}

mkdir -p "$OUT_DIR/after"
iperf3 -B "$IFNAME" -c "$SERVER_IP" -t "$SECONDS" > "$OUT_DIR/after/iperf_output.txt" 2>&1 || true
echo "iperf validation done"
