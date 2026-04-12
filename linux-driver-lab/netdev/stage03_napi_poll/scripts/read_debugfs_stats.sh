#!/usr/bin/env bash
set -euo pipefail

if [[ -f /sys/kernel/debug/netdev_stage03/stats ]]; then
    sudo cat /sys/kernel/debug/netdev_stage03/stats
else
    echo "[stage03] debugfs stats not found: /sys/kernel/debug/netdev_stage03/stats" >&2
    exit 1
fi
