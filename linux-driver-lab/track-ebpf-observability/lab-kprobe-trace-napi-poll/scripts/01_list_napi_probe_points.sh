#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/NAPI_PROBE_POINTS.txt"
{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo
    echo "## bpftrace version"
    bpftrace --version 2>&1 || true
    echo
    echo "## kprobe napi candidates"
    sudo bpftrace -l 'kprobe:*napi*' 2>&1 | head -200 || true
    echo
    echo "## kprobe poll candidates"
    sudo bpftrace -l 'kprobe:*poll*' 2>&1 | head -200 || true
    echo
    echo "## tracepoint irq softirq"
    sudo bpftrace -l 'tracepoint:irq:softirq*' 2>&1 || true
} | tee "${OUT}"
echo "NAPI_PROBE_POINTS=${OUT}"
