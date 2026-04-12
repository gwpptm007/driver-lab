#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT_DIR/output/host_env_stage01.env"

mkdir -p "$ROOT_DIR/output"

has_cmd() {
    command -v "$1" >/dev/null 2>&1 && echo yes || echo no
}

KDIR=${KDIR:-/lib/modules/$(uname -r)/build}

{
    echo "UNAME_R=$(uname -r)"
    echo "HAS_GCC=$(has_cmd gcc)"
    echo "HAS_MAKE=$(has_cmd make)"
    echo "HAS_IP=$(has_cmd ip)"
    echo "HAS_ETHTOOL=$(has_cmd ethtool)"
    echo "HAS_SUDO=$(has_cmd sudo)"
    echo "HAS_DEBUGFS_DIR=$([ -d /sys/kernel/debug ] && echo yes || echo no)"
    echo "KDIR=$KDIR"
    echo "HAS_KERNEL_HEADERS=$([ -d "$KDIR" ] && echo yes || echo no)"
} > "$OUT"

echo "[stage01] host env -> $OUT"
