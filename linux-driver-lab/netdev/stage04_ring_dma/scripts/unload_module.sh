#!/usr/bin/env bash
#
# unload_module.sh — 卸载 netdev_stage04 内核模块
#
# 【注意】
#   rmmod 可能卡死（used=-1），通常是 cleanup 顺序错误导致。
#   stage04_exit 正确的清理顺序：
#     napi_disable → netif_tx_disable → unregister_netdev → remove_pack → cleanup

set -euo pipefail
if lsmod | awk '{print $1}' | grep -qx netdev_stage04; then
    sudo rmmod netdev_stage04
    echo "[stage04] unloaded netdev_stage04"
else
    echo "[stage04] module netdev_stage04 is not loaded"
fi
