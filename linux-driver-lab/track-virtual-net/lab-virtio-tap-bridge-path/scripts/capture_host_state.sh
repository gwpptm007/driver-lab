#!/usr/bin/env bash
set -euo pipefail
OUT=${1:?usage: $0 <record-dir>}
mkdir -p "$OUT"
ip -br link > "$OUT/host_ip_br_link.txt" 2>&1 || true
ip addr > "$OUT/host_ip_addr.txt" 2>&1 || true
bridge link > "$OUT/host_bridge_link.txt" 2>&1 || true
bridge fdb show > "$OUT/host_bridge_fdb.txt" 2>&1 || true
lsmod | grep -E 'tun|bridge|vhost' > "$OUT/host_modules.txt" 2>&1 || true
echo "$OUT"
