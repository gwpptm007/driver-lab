#!/usr/bin/env bash
set -euo pipefail
OUT=${1:?usage: $0 <record-dir>}
mkdir -p "$OUT/final_host_state"

ip -br link > "$OUT/final_host_state/ip_br_link.txt" 2>&1 || true
ip addr > "$OUT/final_host_state/ip_addr.txt" 2>&1 || true
bridge link > "$OUT/final_host_state/bridge_link.txt" 2>&1 || true
bridge fdb show > "$OUT/final_host_state/bridge_fdb.txt" 2>&1 || true
lsmod | grep -E 'vhost|tun|bridge' > "$OUT/final_host_state/modules.txt" 2>&1 || true
ps -ef | grep '[q]emu' > "$OUT/final_host_state/qemu_processes.txt" 2>&1 || true

echo "$OUT/final_host_state"
