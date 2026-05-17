#!/usr/bin/env bash
# 脚本: 00_check_env.sh
# 功能: 检查测试机环境，收集系统信息、DPDK 工具状态
# 用法: ./scripts/00_check_env.sh

set -euo pipefail
# 加载公共函数库
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
# 初始化记录文件（COMMANDS.md、SUMMARY.md、RESULT.md）
init_record_files "${RECORD_DIR}"
# 记录本脚本执行命令到 COMMANDS.md
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

# 收集系统信息：内核版本、OS信息、用户身份、CPU信息、网络接口状态
run_capture "${OUT}" uname -a
run_capture "${OUT}" bash -c 'lsb_release -a 2>/dev/null || cat /etc/os-release'
run_capture "${OUT}" bash -c 'id'
run_capture "${OUT}" bash -c 'nproc; lscpu | sed -n "1,25p"'
run_capture "${OUT}" ip -br addr
run_capture "${OUT}" ip -br link
# 查看 DPDK 网卡和管理网卡的驱动信息
run_capture "${OUT}" bash -c "ethtool -i ${DPDK_IF} 2>/dev/null || true"
run_capture "${OUT}" bash -c "ethtool -i ${MGMT_IF} 2>/dev/null || true"
# 查看 DPDK PCI 设备详情
run_capture "${OUT}" bash -c "lspci -s ${DPDK_PCI} -nn 2>/dev/null || true"
# 列出所有以太网/网络设备
run_capture "${OUT}" bash -c 'lspci | grep -Ei "ethernet|network|vmxnet|e1000" || true'
# 查看大页内存状态
run_capture "${OUT}" bash -c 'grep Huge /proc/meminfo || true'
run_capture "${OUT}" bash -c 'mount | grep -E "hugetlbfs|/mnt/huge" || true'
# 查看已加载的内核模块（vfio/uio/vmxnet3）
run_capture "${OUT}" bash -c 'lsmod | grep -E "vfio|uio|vmxnet3" || true'
# 查看启动命令行参数
run_capture "${OUT}" bash -c 'cat /proc/cmdline || true'
# 查看 dmesg 中与 DPDK/IOMMU/Huge 相关的内核日志
run_capture "${OUT}" bash -c 'dmesg | grep -Ei "DMAR|IOMMU|vfio|vmxnet|huge" | tail -n 80 || true'

{
    echo
    echo "## DPDK tool discovery"
    # 查找 dpdk-devbind.py 并显示当前 DPDK 设备绑定状态
    if devbind="$(find_devbind)"; then
        echo "dpdk-devbind: ${devbind}"
        "${devbind}" --status || true
    else
        echo "dpdk-devbind: NOT FOUND"
    fi
    echo
    # 查找 dpdk-testpmd 并显示版本信息
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
