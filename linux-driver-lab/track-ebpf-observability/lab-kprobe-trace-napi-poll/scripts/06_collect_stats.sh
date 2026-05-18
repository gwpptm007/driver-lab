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
    echo "# 本轮测试生成的所有证据文件。"
    find "${RD}" -maxdepth 1 -type f -printf '%f\n' | sort
    echo
    echo "## iface stats: ${EBPF_IFACE}"
    echo "# 测试结束后的接口计数，用来判断是否真的有流量。"
    ip -s link show dev "${EBPF_IFACE}" 2>&1 || true
    echo
    echo "## softnet_stat first 16 lines"
    echo "# softnet_stat 是每 CPU 网络软中断处理的辅助证据。"
    nl -ba /proc/net/softnet_stat | head -16 || true
    echo
    echo "## dmesg bpf/bpftrace recent"
    dmesg 2>/dev/null | grep -Ei 'bpf|kprobe|trace' | tail -80 || true
} | tee "${OUT}"
echo "COLLECT_STATS=${OUT}"
