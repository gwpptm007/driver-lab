#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
require_root
refuse_management_iface "prepare kernel netdev"

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/PREPARE_KERNEL_NETDEV.txt"

{
    write_env_header
    echo
    echo "== before =="
    ip -br link show "${AF_XDP_IFACE}" 2>/dev/null || echo "iface ${AF_XDP_IFACE}: NOT_FOUND"
    lspci -nnk -s "${AF_XDP_PCI}" || true
    echo

    if [[ -d "/sys/class/net/${AF_XDP_IFACE}" ]]; then
        echo "iface ${AF_XDP_IFACE} already exists; no rebind needed."
    else
        if [[ "${AF_XDP_CONFIRM_REBIND:-NO}" != "YES" ]]; then
            echo "ERROR: iface ${AF_XDP_IFACE} not found."
            echo "To rebind ${AF_XDP_PCI} to ${AF_XDP_DRIVER}, run:"
            echo "  sudo AF_XDP_CONFIRM_REBIND=YES $0"
            exit 2
        fi
        echo "modprobe ${AF_XDP_DRIVER}"
        modprobe "${AF_XDP_DRIVER}" || true
        if command -v dpdk-devbind.py >/dev/null 2>&1; then
            echo "dpdk-devbind.py -b ${AF_XDP_DRIVER} ${AF_XDP_PCI}"
            dpdk-devbind.py -b "${AF_XDP_DRIVER}" "${AF_XDP_PCI}"
        else
            echo "ERROR: dpdk-devbind.py not found; cannot safely rebind automatically."
            exit 3
        fi
        sleep 2
    fi

    echo
    echo "== after =="
    ip -br link show "${AF_XDP_IFACE}" 2>/dev/null || true
    ethtool -i "${AF_XDP_IFACE}" 2>/dev/null || true
    lspci -nnk -s "${AF_XDP_PCI}" || true
    echo
    echo "PREPARE_RESULT=PASS_OR_MANUAL_CHECK"
} 2>&1 | tee "${OUT}"
