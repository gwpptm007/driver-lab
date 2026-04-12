#!/usr/bin/env bash
set -euo pipefail

MODULE_NAME=${MODULE_NAME:-netdev_stage01}

if lsmod | awk '{print $1}' | grep -qx "$MODULE_NAME"; then
    sudo rmmod "$MODULE_NAME"
    echo "[stage01] unloaded module $MODULE_NAME"
else
    echo "[stage01] module $MODULE_NAME not loaded"
fi
