#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(new_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/ENV_CHECK.txt"

{
    echo "# ENV_CHECK"
    echo
    echo "## lab env"
    print_lab_env
    echo
    echo "## date"
    date -Is
    echo
    echo "## os-release"
    cat /etc/os-release 2>/dev/null || true
    echo
    echo "## kernel"
    uname -a
    echo
    echo "## cpu"
    nproc || true
    lscpu 2>/dev/null | sed -n '1,80p' || true
    echo
    echo "## commands"
    for c in gcc pkg-config meson ninja dpdk-devbind.py dpdk-devbind dpdk-testpmd testpmd lspci ethtool ip; do
        if command -v "$c" >/dev/null 2>&1; then
            echo "$c: $(command -v "$c")"
        else
            echo "$c: NOT FOUND"
        fi
    done
    echo
    echo "## pkg-config libdpdk"
    if pkg-config --exists libdpdk; then
        echo "libdpdk: FOUND"
        echo "version: $(pkg-config --modversion libdpdk 2>/dev/null || true)"
        echo "cflags: $(pkg-config --cflags libdpdk 2>/dev/null || true)"
        echo "libs: $(pkg-config --libs libdpdk 2>/dev/null | cut -c1-240 || true) ..."
    else
        echo "libdpdk: NOT FOUND"
    fi
    echo
    echo "## hugepages"
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
    echo
    echo "## interfaces"
    ip -br addr || true
    echo
    echo "## ${DPDK_IF} ethtool"
    ethtool -i "${DPDK_IF}" 2>/dev/null || true
    echo
    echo "## ${MGMT_IF} ethtool"
    ethtool -i "${MGMT_IF}" 2>/dev/null || true
    echo
    echo "## pci"
    lspci -nn | grep -Ei 'ethernet|network|vmxnet|virtio' || true
    echo
    echo "## dpdk-devbind status"
    if devbind="$(find_devbind 2>/dev/null)"; then
        "${devbind}" --status || true
    else
        echo "dpdk-devbind: NOT FOUND"
    fi
} > "${OUT}" 2>&1

append_command_log "${RECORD_DIR}" "$0"

echo "[OK] env check saved: ${OUT}"
echo "[OK] record dir: ${RECORD_DIR}"
