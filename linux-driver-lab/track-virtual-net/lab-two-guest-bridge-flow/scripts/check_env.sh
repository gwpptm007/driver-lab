#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
source "$DIR/common.sh"

say "checking host tools"
for cmd in ip bridge qemu-system-aarch64 tcpdump ps grep; do
    if command -v "$cmd" >/dev/null 2>&1; then
        echo "[ok] $cmd"
    else
        echo "[warn] missing $cmd"
    fi
done

say "checking tun/bridge"
[[ -e /dev/net/tun ]] && echo "[ok] /dev/net/tun exists" || echo "[warn] /dev/net/tun missing"
lsmod | grep -E 'tun|bridge|vhost' || true
