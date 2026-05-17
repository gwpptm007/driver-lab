#!/usr/bin/env bash
# 脚本: 04_collect_stats.sh
# 功能: 收集 DPDK 测试后的网卡状态、统计信息
# 用法: ./scripts/04_collect_stats.sh

set -euo pipefail
# 加载公共函数库
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

devbind="$(find_devbind || true)"

# 辅助函数：将命令输出重定向到指定文件
run_to_file() {
    local file="$1"
    shift
    : > "${file}"
    run_capture "${file}" "$@"
}

# 收集 DPDK 设备绑定状态（如有）
if [[ -n "${devbind}" ]]; then
    run_to_file "${RECORD_DIR}/BIND_STATUS.txt" "${devbind}" --status
else
    echo "dpdk-devbind.py NOT FOUND" > "${RECORD_DIR}/BIND_STATUS.txt"
fi

# 收集大页内存状态
run_to_file "${RECORD_DIR}/HUGEPAGE_STATUS.txt" bash -c 'grep Huge /proc/meminfo; echo; mount | grep hugetlbfs || true'
# 收集 DPDK PCI 设备详细信息
run_to_file "${RECORD_DIR}/PCI_DETAIL.txt" bash -c "lspci -vv -s ${DPDK_PCI} 2>/dev/null || lspci -s ${DPDK_PCI} -nn"
# 收集 IP 地址信息
run_to_file "${RECORD_DIR}/IP_ADDR.txt" ip -br addr
# 收集网络接口详细信息
run_to_file "${RECORD_DIR}/IP_LINK.txt" ip -d link
# 收集网卡驱动信息
run_to_file "${RECORD_DIR}/ETHTOOL_DRIVER.txt" bash -c "ethtool -i ${DPDK_IF} 2>/dev/null || true"
# 收集网卡统计信息（收发包计数等）
run_to_file "${RECORD_DIR}/ETHTOOL_STATS.txt" bash -c "ethtool -S ${DPDK_IF} 2>/dev/null || true"
# 收集 dmesg 中与 DPDK 相关的内核日志
run_to_file "${RECORD_DIR}/DMESG_DPDK_NET.txt" bash -c 'dmesg | grep -Ei "dpdk|vfio|uio|vmxnet|iommu|huge|pci" | tail -n 200 || true'

cat <<EOF

[OK] Stats collected in:
${RECORD_DIR}

Next:
  ./scripts/05_make_review_bundle.sh
EOF
