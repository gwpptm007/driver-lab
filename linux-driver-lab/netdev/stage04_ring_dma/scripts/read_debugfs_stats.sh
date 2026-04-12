#!/usr/bin/env bash
set -euo pipefail
DBG_DIR=/sys/kernel/debug/netdev_stage04
if [[ ! -d "$DBG_DIR" ]]; then
    echo "[stage04] debugfs dir not found: $DBG_DIR" >&2
    exit 1
fi
sudo cat "$DBG_DIR/stats"
echo
sudo cat "$DBG_DIR/rings"
