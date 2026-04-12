#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_FILE="$ROOT_DIR/output/host_env_stage02.env"
KDIR=${KDIR:-/lib/modules/$(uname -r)/build}

has_cmd() {
    command -v "$1" >/dev/null 2>&1 && echo yes || echo no
}

cat > "$OUT_FILE" <<EOF2
UNAME_R=$(uname -r)
HAS_GCC=$(has_cmd gcc)
HAS_MAKE=$(has_cmd make)
HAS_IP=$(has_cmd ip)
HAS_ETHTOOL=$(has_cmd ethtool)
HAS_SUDO=$(has_cmd sudo)
HAS_TIMEOUT=$(has_cmd timeout)
HAS_DEBUGFS_DIR=$([ -d /sys/kernel/debug ] && echo yes || echo no)
HAS_KERNEL_HEADERS=$([ -d "$KDIR" ] && echo yes || echo no)
KDIR=$KDIR
EOF2

echo "[stage02] host env -> $OUT_FILE"
