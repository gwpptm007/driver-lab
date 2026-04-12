#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
MODULE_PATH="$ROOT_DIR/output/netdev_stage02.ko"
IFNAME=${IFNAME:-nds2}
LOOP_MODE=${LOOP_MODE:-copy}

if [[ ! -f "$MODULE_PATH" ]]; then
    echo "[stage02] missing $MODULE_PATH, run 'make build-module' first" >&2
    exit 1
fi

sudo insmod "$MODULE_PATH" ifname="$IFNAME" loop_mode="$LOOP_MODE"
echo "[stage02] module loaded: ifname=$IFNAME loop_mode=$LOOP_MODE"
