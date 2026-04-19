#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
KO="$ROOT_DIR/output/netdev_stage09.ko"
ACTION=${1:-reload}

IFNAME=${IFNAME:-nds9}
NUM_QUEUES=${NUM_QUEUES:-2}
RING_SIZE=${RING_SIZE:-128}
NAPI_WEIGHT=${NAPI_WEIGHT:-64}
RX_BUF_SIZE=${RX_BUF_SIZE:-2048}
BACKEND_DELAY_US=${BACKEND_DELAY_US:-0}
BACKEND_BATCH=${BACKEND_BATCH:-64}

is_loaded() {
    lsmod | awk '{print $1}' | grep -qx netdev_stage09
}

case "$ACTION" in
    load)
        test -f "$KO" || { echo "[stage09] missing $KO, run scripts/build.sh first" >&2; exit 1; }
        if is_loaded; then
            echo "[stage09] netdev_stage09 already loaded"
            exit 0
        fi
        sudo insmod "$KO" ifname="$IFNAME" num_queues="$NUM_QUEUES" ring_size="$RING_SIZE" napi_weight="$NAPI_WEIGHT" \
            rx_buf_size="$RX_BUF_SIZE" backend_delay_us="$BACKEND_DELAY_US" backend_batch="$BACKEND_BATCH"
        sleep 1
        sudo ip link set dev "$IFNAME" up || true
        ip -details link show "$IFNAME" || true
        ;;
    unload)
        if is_loaded; then
            sudo rmmod netdev_stage09
            echo "[stage09] unloaded"
        else
            echo "[stage09] module not loaded"
        fi
        ;;
    reload)
        "$0" unload || true
        "$0" load
        ;;
    status)
        is_loaded && echo "[stage09] loaded" || echo "[stage09] not loaded"
        ip -details link show "$IFNAME" || true
        ;;
    *)
        echo "Usage: $0 [load|unload|reload|status]" >&2
        exit 1
        ;;
esac
