#!/usr/bin/env bash
set -euo pipefail

if lsmod | awk '{print $1}' | grep -qx netdev_stage03; then
    sudo rmmod netdev_stage03
    echo "[stage03] module unloaded"
else
    echo "[stage03] module netdev_stage03 is not loaded"
fi
