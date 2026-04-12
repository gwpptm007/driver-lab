#!/usr/bin/env bash
set -euo pipefail

if lsmod | awk '{print $1}' | grep -qx netdev_stage02; then
    sudo rmmod netdev_stage02
    echo "[stage02] module unloaded"
else
    echo "[stage02] module netdev_stage02 is not loaded"
fi
