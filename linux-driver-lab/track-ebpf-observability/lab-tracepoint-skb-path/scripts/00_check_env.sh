#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(make_record_dir)"
OUT="${RD}/ENV_CHECK.txt"
{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo "KERNEL=$(uname -r)"
    echo "EBPF_IFACE=${EBPF_IFACE}"
    echo "EBPF_MGMT_IFACE=${EBPF_MGMT_IFACE}"
    echo
    echo "## tools"
    for t in bpftrace ip ethtool timeout; do
        if command -v "$t" >/dev/null 2>&1; then
            echo "$t: $(command -v "$t")"
        else
            echo "$t: NOT_FOUND"
        fi
    done
    echo
    echo "## iface"
    ip -br link show dev "${EBPF_IFACE}" 2>&1 || true
    ip -s link show dev "${EBPF_IFACE}" 2>&1 || true
    echo
    echo "## driver"
    ethtool -i "${EBPF_IFACE}" 2>&1 || true
    echo
    echo "## kernel knobs"
    cat /proc/sys/kernel/kptr_restrict 2>/dev/null | sed 's/^/kptr_restrict=/' || true
    cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null | sed 's/^/perf_event_paranoid=/' || true
    echo
    echo "## tracefs mounts"
    mount | grep tracefs 2>&1 || true
    ls -d /sys/kernel/debug/tracing/events/net/ 2>/dev/null | sed 's/^/trace_events_net_dir=/' || true
    ls -d /sys/kernel/debug/tracing/events/skb/ 2>/dev/null | sed 's/^/trace_events_skb_dir=/' || true
} | tee "${OUT}"
echo "ENV_CHECK=${OUT}"
