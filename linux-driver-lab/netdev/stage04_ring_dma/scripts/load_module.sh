#!/usr/bin/env bash
#
# load_module.sh — 加载 netdev_stage04 内核模块
#
# 【模块参数】
#   ifname=xxx     net_device 名称（默认 nds4）
#   ring_size=N    TX/RX ring 深度（默认 64）
#   napi_weight=N  NAPI poll weight（默认 16）
#   rx_buf_size=N  预分配 RX buffer 大小（默认 2048）
#
# 【可通过环境变量覆盖】
#   IFNAME=xxx RING_SIZE=32 NAPI_WEIGHT=8 ./load_module.sh
#
# 【注意】
#   如果模块已加载，会先 rmmod 再 insmod（确保参数生效）

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
KO="$ROOT_DIR/output/netdev_stage04.ko"
IFNAME=${IFNAME:-nds4}
RING_SIZE=${RING_SIZE:-64}
NAPI_WEIGHT=${NAPI_WEIGHT:-16}
RX_BUF_SIZE=${RX_BUF_SIZE:-2048}

if [[ ! -f "$KO" ]]; then
    echo "[stage04] missing $KO, run 'make build-module' first" >&2
    exit 1
fi

if lsmod | awk '{print $1}' | grep -qx netdev_stage04; then
    echo "[stage04] module already loaded, unloading first"
    sudo rmmod netdev_stage04 || true
fi

sudo insmod "$KO" ifname="$IFNAME" ring_size="$RING_SIZE" napi_weight="$NAPI_WEIGHT" rx_buf_size="$RX_BUF_SIZE"
sleep 1
ip -details link show "$IFNAME" || true
echo "[stage04] loaded netdev_stage04 ifname=$IFNAME ring_size=$RING_SIZE napi_weight=$NAPI_WEIGHT rx_buf_size=$RX_BUF_SIZE"
