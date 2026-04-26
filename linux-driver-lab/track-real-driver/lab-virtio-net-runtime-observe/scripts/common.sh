#!/usr/bin/env bash
set -euo pipefail

tracefs_dir() {
    if [[ -d /sys/kernel/tracing ]]; then
        echo /sys/kernel/tracing
    elif [[ -d /sys/kernel/debug/tracing ]]; then
        echo /sys/kernel/debug/tracing
    else
        return 1
    fi
}

now_ts() {
    date +%Y%m%d_%H%M%S
}

ensure_dir() {
    mkdir -p "$1"
}
