#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
MODULE_PATH="$ROOT_DIR/output/netdev_stage03.ko"
IFNAME=${IFNAME:-nds3}
LOOP_MODE=${LOOP_MODE:-copy}
RX_MODE=${RX_MODE:-napi}
NAPI_WEIGHT=${NAPI_WEIGHT:-8}
MAX_QUEUE_DEPTH=${MAX_QUEUE_DEPTH:-1024}

if [[ ! -f "$MODULE_PATH" ]]; then
    echo "[stage03] missing $MODULE_PATH, run 'make build-module' first" >&2
    exit 1
fi

sudo insmod "$MODULE_PATH" \
    ifname="$IFNAME" \
    loop_mode="$LOOP_MODE" \
    rx_mode="$RX_MODE" \
    napi_weight="$NAPI_WEIGHT" \
    max_queue_depth="$MAX_QUEUE_DEPTH"

echo "[stage03] module loaded: ifname=$IFNAME loop_mode=$LOOP_MODE rx_mode=$RX_MODE napi_weight=$NAPI_WEIGHT max_queue_depth=$MAX_QUEUE_DEPTH"
