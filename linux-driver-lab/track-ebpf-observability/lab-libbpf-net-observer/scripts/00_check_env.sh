#!/usr/bin/env bash
set -euo pipefail

LAB_NAME="lab-libbpf-net-observer"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${LAB_DIR}/records"
: "${EBPF_IFACE:=ens33}"
: "${EBPF_MGMT_IFACE:=ens33}"

mkdir -p "${RECORD_ROOT}"

main() {
    local d="${RECORD_ROOT}/$(date +%Y%m%d-%H%M%S)-libbpf-net-observer"
    mkdir -p "${d}"
    printf '%s\n' "${d}" > "${RECORD_ROOT}/.last_record_dir"

    local out="${d}/ENV_CHECK.txt"
    {
        echo "LAB=${LAB_NAME}"
        echo "DATE=$(date -Iseconds)"
        echo "KERNEL=$(uname -r)"
        echo
        echo "## build tools"
        for t in gcc clang bpftool llvm-strip make; do
            if command -v "$t" >/dev/null 2>&1; then
                echo "$t: $(command -v "$t") ($($t --version 2>&1 | head -1))"
            else
                echo "$t: NOT_FOUND"
            fi
        done
        echo
        echo "## libbpf"
        dpkg -l libbpf-dev 2>/dev/null | grep libbpf-dev || echo "libbpf-dev: NOT_FOUND"
        echo
        echo "## BTF"
        ls -lh /sys/kernel/btf/vmlinux 2>/dev/null || echo "BTF: NOT_FOUND"
        echo
        echo "## trace events"
        ls /sys/kernel/debug/tracing/events/net/netif_receive_skb/ 2>/dev/null || echo "netif_receive_skb: NOT_FOUND"
        ls /sys/kernel/debug/tracing/events/skb/kfree_skb/ 2>/dev/null || echo "kfree_skb: NOT_FOUND"
        echo
        echo "## iface"
        ip -br link show dev "${EBPF_IFACE}" 2>&1 || true
        ip -s link show dev "${EBPF_IFACE}" 2>&1 || true
        echo
        echo "## kernel knobs"
        cat /proc/sys/kernel/kptr_restrict 2>/dev/null | sed 's/^/kptr_restrict=/' || true
        cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null | sed 's/^/perf_event_paranoid=/' || true
    } | tee "${out}"

    echo "[env] record dir: ${d}"
}

main "$@"
