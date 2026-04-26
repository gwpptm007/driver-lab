#!/usr/bin/env bash
set -euo pipefail

default_kernel_src() {
    echo "${KERNEL_SRC:-/home/wq7/workspace/kernel-src/linux-5.15.10}"
}

ts_now() {
    date +%Y%m%d_%H%M%S
}

ensure_dir() {
    mkdir -p "$1"
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "missing command: $1" >&2
        exit 1
    }
}
