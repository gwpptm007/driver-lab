#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

REC_DIR="$(make_record_dir)"
OUT="${REC_DIR}/ENV_CHECK.txt"

{
    write_env_header
    echo
    echo "== toolchain =="
    for c in clang llvm-objdump cc make pkg-config bpftool ip ethtool lspci; do
        if command -v "${c}" >/dev/null 2>&1; then
            echo "${c}: $(command -v "${c}")"
            case "${c}" in
                clang) clang --version | head -1 ;;
                bpftool) bpftool version 2>/dev/null | head -5 || true ;;
            esac
        else
            echo "${c}: NOT_FOUND"
        fi
    done
    echo
    echo "== libbpf =="
    if pkg-config --exists libbpf 2>/dev/null; then
        echo "libbpf.pc: FOUND"
        echo "libbpf version: $(pkg-config --modversion libbpf)"
        echo "libbpf cflags: $(pkg-config --cflags libbpf)"
        echo "libbpf libs: $(pkg-config --libs libbpf)"
    else
        echo "libbpf.pc: NOT_FOUND"
        echo "hint: sudo apt install libbpf-dev libelf-dev zlib1g-dev"
    fi
    echo
    echo "== kernel bpf/xdp =="
    uname -a
    if [[ -d /sys/fs/bpf ]]; then
        echo "/sys/fs/bpf: EXISTS"
        mount | grep -E ' /sys/fs/bpf ' || echo "/sys/fs/bpf not mounted as bpffs?"
    else
        echo "/sys/fs/bpf: MISSING"
    fi
    echo
    echo "== iface =="
    if [[ -d "/sys/class/net/${AF_XDP_IFACE}" ]]; then
        echo "iface ${AF_XDP_IFACE}: FOUND"
        ip -br link show "${AF_XDP_IFACE}" || true
        ethtool -i "${AF_XDP_IFACE}" 2>/dev/null || true
        echo "queues:"
        ls -d "/sys/class/net/${AF_XDP_IFACE}/queues"/* 2>/dev/null || true
    else
        echo "iface ${AF_XDP_IFACE}: NOT_FOUND"
        echo "If you just ran DPDK, ${AF_XDP_PCI} may still be bound to uio/vfio."
    fi
    echo
    echo "== pci =="
    lspci -nnk -s "${AF_XDP_PCI}" 2>/dev/null || true
    if command -v dpdk-devbind.py >/dev/null 2>&1; then
        echo
        echo "== dpdk-devbind =="
        dpdk-devbind.py --status 2>/dev/null | sed -n '1,120p' || true
    fi
    echo
    echo "RECORD_DIR=${REC_DIR}"
} | tee "${OUT}"
