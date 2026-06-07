#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/TRACEPOINT_LIST.txt"
{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo
    echo "## bpftrace version"
    bpftrace --version 2>&1 || true
    echo
    echo "## net tracepoints (rx/tx paths)"
    echo "# 这里列出与 skb RX/TX 路径相关的 net tracepoint。"
    sudo bpftrace -l 'tracepoint:net:*' 2>&1 | grep -iE 'receive|dev_queue|dev_start_xmit|gro|napi' | head -80 || true
    echo
    echo "## skb tracepoints (free/drop/consume)"
    sudo bpftrace -l 'tracepoint:skb:*' 2>&1 | head -40 || true
    echo
    echo "## all net tracepoints raw (first 100)"
    sudo bpftrace -l 'tracepoint:net:*' 2>&1 | head -100 || true
} | tee "${OUT}"
echo "TRACEPOINT_LIST=${OUT}"
