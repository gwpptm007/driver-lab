#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
MODULE_NAME=${MODULE_NAME:-netdev_stage01}
IFNAME=${IFNAME:-nds0}
KO="$ROOT_DIR/output/${MODULE_NAME}.ko"

if [[ ! -f "$KO" ]]; then
    echo "[stage01] missing module: $KO" >&2
    echo "[stage01] run 'make build-module' first" >&2
    exit 1
fi

sudo insmod "$KO" ifname="$IFNAME"
echo "[stage01] loaded module $MODULE_NAME ifname=$IFNAME"
