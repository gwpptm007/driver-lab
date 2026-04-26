#!/usr/bin/env bash
set -euo pipefail
OUT=${1:?usage: $0 <out-dir>}
mkdir -p "$OUT"

ip -br link > "$OUT/ip_br_link.txt" 2>&1 || true
ip addr > "$OUT/ip_addr.txt" 2>&1 || true
ip -s link > "$OUT/ip_s_link.txt" 2>&1 || true
bridge link > "$OUT/bridge_link.txt" 2>&1 || true
bridge fdb show > "$OUT/bridge_fdb.txt" 2>&1 || true
lsmod | grep -E 'vhost|tun|bridge|tap' > "$OUT/modules.txt" 2>&1 || true
ls -l /dev/vhost-net /dev/net/tun > "$OUT/dev_nodes.txt" 2>&1 || true
ps -ef | grep '[q]emu' > "$OUT/qemu_process.txt" 2>&1 || true
dmesg | tail -n 120 > "$OUT/dmesg_tail.txt" 2>&1 || true

echo "$OUT"
