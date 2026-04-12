#!/usr/bin/env bash
set -euo pipefail

STATS_PATH=${DEBUGFS_STATS:-/sys/kernel/debug/netdev_stage02/stats}

if [[ ! -f "$STATS_PATH" ]]; then
    echo "[stage02] debugfs stats not found: $STATS_PATH" >&2
    exit 1
fi

sudo cat "$STATS_PATH"
