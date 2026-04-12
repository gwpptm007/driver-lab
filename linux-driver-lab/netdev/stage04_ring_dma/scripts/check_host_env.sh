#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_FILE="$ROOT_DIR/output/host_env_stage04.env"
KDIR=${KDIR:-/lib/modules/$(uname -r)/build}
TIMEOUT_BIN=${TIMEOUT_BIN:-timeout}

mkdir -p "$ROOT_DIR/output"

has_cmd() {
    command -v "$1" >/dev/null 2>&1 && echo yes || echo no
}

{
    printf "HOST_UNAME=%q\n" "$(uname -a | sed 's/[[:space:]]\+/ /g')"
    echo "HOST_KERNEL=$(uname -r)"
    echo "HOST_CC=${HOST_CC:-gcc}"
    printf "KDIR=%q\n" "$KDIR"
    if [[ -d "$KDIR" ]]; then
        echo "KDIR_OK=yes"
    else
        echo "KDIR_OK=no"
    fi
    echo "HAVE_GCC=$(has_cmd gcc)"
    echo "HAVE_TIMEOUT=$(has_cmd "$TIMEOUT_BIN")"
    echo "HAVE_IP=$(has_cmd ip)"
    echo "HAVE_LSMOD=$(has_cmd lsmod)"
    echo "HAVE_ETHTOOL=$(has_cmd ethtool)"
    echo "HAVE_SUDO=$(has_cmd sudo)"
    if [[ -d /sys/kernel/debug ]]; then
        echo "DEBUGFS_ROOT=yes"
    else
        echo "DEBUGFS_ROOT=no"
    fi
} > "$OUT_FILE"

echo "[stage04] host env -> $OUT_FILE"
