#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(new_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/ENV_CHECK.txt"
append_command_log "${RECORD_DIR}" "$0"

{
    echo "# ENV_CHECK"
    echo
    echo "## project env"
    print_project_env
    echo
    echo "## OS"
    cat /etc/os-release 2>/dev/null || true
    echo
    uname -a
    echo
    echo "## CPU"
    nproc || true
    lscpu | sed -n '1,30p' || true
    echo
    echo "## commands"
    for c in gcc pkg-config meson ninja make python3 ip lspci; do
        if need_cmd "${c}"; then
            echo "${c}: $(command -v "${c}")"
        else
            echo "${c}: NOT FOUND"
        fi
    done
    if pkg-config --exists libdpdk 2>/dev/null; then
        echo "libdpdk: $(pkg-config --modversion libdpdk)"
    else
        echo "libdpdk: NOT FOUND by pkg-config"
    fi
    if devbind="$(find_devbind 2>/dev/null)"; then
        echo "dpdk-devbind: ${devbind}"
    else
        echo "dpdk-devbind: NOT FOUND"
    fi
    echo
    echo "## hugepages"
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
    mount | grep hugetlbfs || true
    echo
    echo "## network"
    ip -br addr || true
    echo
    echo "## PCI"
    lspci -nn | grep -Ei 'ethernet|network|vmxnet3' || true
    echo
    echo "## dpdk-devbind status"
    if devbind="$(find_devbind 2>/dev/null)"; then
        "${devbind}" --status || true
    fi
} > "${OUT}" 2>&1

echo "[OK] env check saved: ${OUT}"
