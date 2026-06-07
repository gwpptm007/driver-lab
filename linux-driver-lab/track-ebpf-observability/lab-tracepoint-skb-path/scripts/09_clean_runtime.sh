#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
pkill -f 'bpftrace.*skb' 2>/dev/null || true
pkill -f 'bpftrace.*tracepoint' 2>/dev/null || true
echo "cleanup done"
