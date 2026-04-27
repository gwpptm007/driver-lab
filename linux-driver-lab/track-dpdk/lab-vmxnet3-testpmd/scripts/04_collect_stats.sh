#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

devbind="$(find_devbind || true)"

run_to_file() {
    local file="$1"
    shift
    : > "${file}"
    run_capture "${file}" "$@"
}

if [[ -n "${devbind}" ]]; then
    run_to_file "${RECORD_DIR}/BIND_STATUS.txt" "${devbind}" --status
else
    echo "dpdk-devbind.py NOT FOUND" > "${RECORD_DIR}/BIND_STATUS.txt"
fi

run_to_file "${RECORD_DIR}/HUGEPAGE_STATUS.txt" bash -c 'grep Huge /proc/meminfo; echo; mount | grep hugetlbfs || true'
run_to_file "${RECORD_DIR}/PCI_DETAIL.txt" bash -c "lspci -vv -s ${DPDK_PCI} 2>/dev/null || lspci -s ${DPDK_PCI} -nn"
run_to_file "${RECORD_DIR}/IP_ADDR.txt" ip -br addr
run_to_file "${RECORD_DIR}/IP_LINK.txt" ip -d link
run_to_file "${RECORD_DIR}/ETHTOOL_DRIVER.txt" bash -c "ethtool -i ${DPDK_IF} 2>/dev/null || true"
run_to_file "${RECORD_DIR}/ETHTOOL_STATS.txt" bash -c "ethtool -S ${DPDK_IF} 2>/dev/null || true"
run_to_file "${RECORD_DIR}/DMESG_DPDK_NET.txt" bash -c 'dmesg | grep -Ei "dpdk|vfio|uio|vmxnet|iommu|huge|pci" | tail -n 200 || true'

cat <<EOF

[OK] Stats collected in:
${RECORD_DIR}

Next:
  ./scripts/05_make_review_bundle.sh
EOF
