#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# run.sh — 加载/卸载 stage11_soft 驱动
#
# 纯软版本，不需要 QEMU！直接在当前 Linux 环境运行。
# module_init → register_netdev 即可，无需 PCI。

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
STAGE11_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
KO="$STAGE11_DIR/output/netdev_stage11_soft.ko"
ACTION=${1:-reload}

IFNAME=${IFNAME:-nds11s}
NUM_QUEUES=${NUM_QUEUES:-2}
RING_SIZE=${RING_SIZE:-128}
NAPI_WEIGHT=${NAPI_WEIGHT:-64}
RX_BUF_SIZE=${RX_BUF_SIZE:-2048}
BACKEND_DELAY_US=${BACKEND_DELAY_US:-0}
BACKEND_BATCH=${BACKEND_BATCH:-64}

is_loaded() {
    lsmod | awk '{print $1}' | grep -qx netdev_stage11_soft
}

case "$ACTION" in
    load)
        test -f "$KO" || { echo "[stage11_soft] missing $KO, run scripts/build.sh first" >&2; exit 1; }
        if is_loaded; then
            echo "[stage11_soft] netdev_stage11_soft already loaded"
            exit 0
        fi
        sudo insmod "$KO" ifname="$IFNAME" num_queues="$NUM_QUEUES" ring_size="$RING_SIZE" \
            napi_weight="$NAPI_WEIGHT" rx_buf_size="$RX_BUF_SIZE" \
            backend_delay_us="$BACKEND_DELAY_US" backend_batch="$BACKEND_BATCH"
        sleep 1
        sudo ip link set dev "$IFNAME" up || true
        ip -details link show "$IFNAME" || true
        echo "[stage11_soft] debugfs:"
        ls /sys/kernel/debug/netdev_stage11_soft/ 2>/dev/null || echo "  (not mounted)"
        ;;
    unload)
        if is_loaded; then
            sudo rmmod netdev_stage11_soft
            echo "[stage11_soft] unloaded"
        else
            echo "[stage11_soft] module not loaded"
        fi
        ;;
    reload)
        "$0" unload 2>/dev/null || true
        "$0" load
        ;;
    status)
        is_loaded && echo "[stage11_soft] loaded" || echo "[stage11_soft] not loaded"
        ip -details link show "$IFNAME" 2>/dev/null || true
        echo "--- debugfs ---"
        sudo cat /sys/kernel/debug/netdev_stage11_soft/stats 2>/dev/null || echo "(not available)"
        ;;
    *)
        echo "Usage: $0 [load|unload|reload|status]" >&2
        echo ""
        echo "Environment variables:"
        echo "  IFNAME=$IFNAME"
        echo "  NUM_QUEUES=$NUM_QUEUES"
        echo "  RING_SIZE=$RING_SIZE"
        echo "  NAPI_WEIGHT=$NAPI_WEIGHT"
        echo "  BACKEND_DELAY_US=$BACKEND_DELAY_US"
        echo "  BACKEND_BATCH=$BACKEND_BATCH"
        exit 1
        ;;
esac