#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/COLLECT_STATS.txt"
{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo "RECORD_DIR=${RD}"
    echo
    echo "## generated files"
    find "${RD}" -maxdepth 1 -type f -printf '%f\n' | sort
    echo
    echo "## iface stats: ${EBPF_IFACE}"
    ip -s link show dev "${EBPF_IFACE}" 2>&1 || true
    echo
    echo "## softnet_stat first 16 lines"
    nl -ba /proc/net/softnet_stat | head -16 || true
    echo
    echo "## tracefs events/net (确认 tracepoint 是否暴露)"
    ls /sys/kernel/debug/tracing/events/net/ 2>/dev/null | head -40 || echo "NO_DEBUG_FS_EVENTS_NET"
    echo
    echo "## tracefs events/skb"
    ls /sys/kernel/debug/tracing/events/skb/ 2>/dev/null | head -20 || echo "NO_DEBUG_FS_EVENTS_SKB"
    echo
    echo "## dmesg bpf/bpftrace recent"
    dmesg 2>/dev/null | grep -Ei 'bpf|kprobe|trace|skb' | tail -80 || true
} | tee "${OUT}"
echo "COLLECT_STATS=${OUT}"
