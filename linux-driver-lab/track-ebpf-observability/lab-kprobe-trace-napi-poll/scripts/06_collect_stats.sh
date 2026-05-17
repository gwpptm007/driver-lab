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
    echo "## dmesg bpf/bpftrace recent"
    dmesg 2>/dev/null | grep -Ei 'bpf|kprobe|trace' | tail -80 || true
} | tee "${OUT}"
echo "COLLECT_STATS=${OUT}"
