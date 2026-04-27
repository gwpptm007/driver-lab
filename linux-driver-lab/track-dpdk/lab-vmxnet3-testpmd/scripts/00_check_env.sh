#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/ENV_CHECK.txt"
: > "${OUT}"

{
    echo "# ENV_CHECK"
    echo "date=$(date '+%F %T')"
    echo
    echo "## Lab defaults"
    print_lab_env
    echo
} >> "${OUT}"

run_capture "${OUT}" uname -a
run_capture "${OUT}" bash -c 'lsb_release -a 2>/dev/null || cat /etc/os-release'
run_capture "${OUT}" bash -c 'id'
run_capture "${OUT}" bash -c 'nproc; lscpu | sed -n "1,25p"'
run_capture "${OUT}" ip -br addr
run_capture "${OUT}" ip -br link
run_capture "${OUT}" bash -c "ethtool -i ${DPDK_IF} 2>/dev/null || true"
run_capture "${OUT}" bash -c "ethtool -i ${MGMT_IF} 2>/dev/null || true"
run_capture "${OUT}" bash -c "lspci -s ${DPDK_PCI} -nn 2>/dev/null || true"
run_capture "${OUT}" bash -c 'lspci | grep -Ei "ethernet|network|vmxnet|e1000" || true'
run_capture "${OUT}" bash -c 'grep Huge /proc/meminfo || true'
run_capture "${OUT}" bash -c 'mount | grep -E "hugetlbfs|/mnt/huge" || true'
run_capture "${OUT}" bash -c 'lsmod | grep -E "vfio|uio|vmxnet3" || true'
run_capture "${OUT}" bash -c 'cat /proc/cmdline || true'
run_capture "${OUT}" bash -c 'dmesg | grep -Ei "DMAR|IOMMU|vfio|vmxnet|huge" | tail -n 80 || true'

{
    echo
    echo "## DPDK tool discovery"
    if devbind="$(find_devbind)"; then
        echo "dpdk-devbind: ${devbind}"
        "${devbind}" --status || true
    else
        echo "dpdk-devbind: NOT FOUND"
    fi
    echo
    if testpmd="$(find_testpmd)"; then
        echo "testpmd: ${testpmd}"
        "${testpmd}" --version 2>/dev/null || true
    else
        echo "testpmd: NOT FOUND"
    fi
} >> "${OUT}" 2>&1

cat <<EOF

[OK] Environment check saved:
${OUT}

Next:
  sudo ./scripts/01_setup_hugepages.sh
EOF
