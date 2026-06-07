#!/usr/bin/env bash
set -euo pipefail
pkill -f skb_observer 2>/dev/null || true
echo "cleanup done"
