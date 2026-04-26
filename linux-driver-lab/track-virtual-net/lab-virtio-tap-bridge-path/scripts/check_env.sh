#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
source "$DIR/common.sh"

say "checking host tools"
for cmd in ip bridge qemu-system-aarch64 tcpdump; do
    if command -v "$cmd" >/dev/null 2>&1; then
        echo "[ok] $cmd"
    else
        echo "[warn] missing $cmd"
    fi
done

say "checking kernel modules"
lsmod | grep -E 'tun|bridge' || true
echo "[*] if /dev/net/tun is missing, load tun: sudo modprobe tun"
