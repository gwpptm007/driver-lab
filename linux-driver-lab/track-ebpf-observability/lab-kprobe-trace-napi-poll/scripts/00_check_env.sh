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
    echo "# 检查观测链路依赖的命令是否存在。"
    for t in bpftrace ip ethtool timeout; do
        if command -v "$t" >/dev/null 2>&1; then
            echo "$t: $(command -v "$t")"
        else
            echo "$t: NOT_FOUND"
        fi
    done
    echo
    echo "## iface"
    echo "# 目标接口状态和收发包基线。"
    ip -br link show dev "${EBPF_IFACE}" 2>&1 || true
    ip -s link show dev "${EBPF_IFACE}" 2>&1 || true
    echo
    echo "## driver"
    echo "# 驱动名会影响可选的 driver poll 符号。"
    ethtool -i "${EBPF_IFACE}" 2>&1 || true
    echo
    echo "## kernel knobs"
    echo "# 这些内核开关会影响 bpftrace/kprobe 可观测性。"
    cat /proc/sys/kernel/kptr_restrict 2>/dev/null | sed 's/^/kptr_restrict=/' || true
    cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null | sed 's/^/perf_event_paranoid=/' || true
    echo
    echo "## xdp status"
    ip -details link show dev "${EBPF_IFACE}" 2>&1 | sed -n '1,20p' || true
} | tee "${OUT}"
echo "ENV_CHECK=${OUT}"
