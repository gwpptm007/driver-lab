#!/usr/bin/env bash
set -euo pipefail

DBG_DIR=/sys/kernel/debug/netdev_stage08
test -d "$DBG_DIR" || { echo "[stage08] debugfs dir not found: $DBG_DIR" >&2; exit 1; }
test -f "$DBG_DIR/timeline" || { echo "[stage08] missing $DBG_DIR/timeline" >&2; exit 1; }

sudo cat "$DBG_DIR/timeline"
