#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
# bpftrace usually exits after timeout. This is only a safe cleanup helper.
pkill -f 'bpftrace.*napi_poll' 2>/dev/null || true
pkill -f 'bpftrace.*softirq_napi' 2>/dev/null || true
echo "cleanup done"
