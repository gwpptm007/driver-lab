#!/usr/bin/env bash
set -euo pipefail

ts_now() {
    date +%Y%m%d_%H%M%S
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "missing command: $1" >&2
        exit 1
    }
}

say() {
    echo "[*] $*"
}
