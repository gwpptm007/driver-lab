#!/usr/bin/env bash
set -euo pipefail
IFNAME=${1:?usage: $0 <ifname> <peer-ip> <record-dir> [count]}
PEER_IP=${2:?usage: $0 <ifname> <peer-ip> <record-dir> [count]}
OUT_DIR=${3:?usage: $0 <ifname> <peer-ip> <record-dir> [count]}
COUNT=${4:-20}

mkdir -p "$OUT_DIR/after"
ping -I "$IFNAME" -c "$COUNT" "$PEER_IP" > "$OUT_DIR/after/ping_output.txt" 2>&1 || true
echo "ping validation done"
