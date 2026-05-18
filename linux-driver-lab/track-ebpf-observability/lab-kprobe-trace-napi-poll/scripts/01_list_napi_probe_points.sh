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
    echo "# 先列出内核实际暴露的 NAPI 相关 kprobe，后续脚本会从这些符号中动态选择。"
    sudo bpftrace -l 'kprobe:*napi*' 2>&1 | head -200 || true
    echo
    echo "## kprobe poll candidates"
    sudo bpftrace -l 'kprobe:*poll*' 2>&1 | head -200 || true
    echo
    echo "## tracepoint irq softirq"
    echo "# NET_RX softirq 使用 irq:softirq_entry/exit 观测。"
    sudo bpftrace -l 'tracepoint:irq:softirq*' 2>&1 || true
} | tee "${OUT}"
echo "NAPI_PROBE_POINTS=${OUT}"
