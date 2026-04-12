#!/usr/bin/env bash
set -euo pipefail

STATS=${DEBUGFS_STATS:-/sys/kernel/debug/netdev_stage01/stats}

if [[ ! -f "$STATS" ]]; then
    echo "[stage01] debugfs stats not found: $STATS" >&2
    exit 1
fi

cat "$STATS"
