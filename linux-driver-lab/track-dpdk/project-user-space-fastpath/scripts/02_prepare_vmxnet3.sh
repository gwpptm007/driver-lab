#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

require_root_for_write
guard_not_mgmt_pci

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/PREPARE_VMXNET3.txt"
: > "${OUT}"
append_command_log "${RECORD_DIR}" "sudo" "$0"

if ! devbind="$(find_devbind 2>/dev/null)"; then
    echo "ERROR: dpdk-devbind not found" | tee -a "${OUT}"
    exit 1
fi

{
    echo "# PREPARE_VMXNET3"
    echo
    echo "## env"
    print_project_env
    echo
    echo "## setup hugepages"
    setup_hugepages
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
    mount | grep hugetlbfs || true
    echo
    echo "## load driver"
    if [[ "${DPDK_DRIVER}" == "uio_pci_generic" ]]; then
        modprobe uio || true
        modprobe uio_pci_generic || true
    elif [[ "${DPDK_DRIVER}" == "vfio-pci" ]]; then
        modprobe vfio-pci || true
    else
        modprobe "${DPDK_DRIVER}" || true
    fi
    lsmod | grep -E 'uio|vfio' || true
    echo
    echo "## before bind"
    "${devbind}" --status || true
    echo
    echo "## bring DPDK interface down if exists"
    ip link set "${DPDK_IF}" down 2>/dev/null || true
    echo
    echo "## bind ${DPDK_PCI} to ${DPDK_DRIVER}"
    "${devbind}" --bind="${DPDK_DRIVER}" "${DPDK_PCI}"
    echo
    echo "## after bind"
    "${devbind}" --status || true
} >> "${OUT}" 2>&1

echo "[OK] prepare saved: ${OUT}"
