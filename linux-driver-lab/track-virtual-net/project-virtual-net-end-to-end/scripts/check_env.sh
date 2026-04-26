#!/usr/bin/env bash
set -euo pipefail
DIR=$(cd -- "$(dirname -- "$0")" && pwd)
source "$DIR/common.sh"

for cmd in ip bridge qemu-system-aarch64 tcpdump grep find; do
    if command -v "$cmd" >/dev/null 2>&1; then
        echo "[ok] $cmd"
    else
        echo "[warn] missing $cmd"
    fi
done
