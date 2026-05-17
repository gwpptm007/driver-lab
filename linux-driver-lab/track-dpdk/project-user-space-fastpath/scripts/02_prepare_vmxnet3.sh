#!/usr/bin/env bash
# 脚本: 02_prepare_vmxnet3.sh
# 功能: 准备 VMXNET3 网卡供 DPDK 使用
#        1. 配置大页内存
#        2. 加载 DPDK 用户态驱动（uio_pci_generic 或 vfio-pci）
#        3. 将 VMXNET3 网卡绑定到 DPDK 驱动
# 用法: sudo ./scripts/02_prepare_vmxnet3.sh
#
# ========== 为什么要绑定到 DPDK 驱动 ==========
# Linux 默认用内核驱动（vmxnet3）管理网卡，DPDK 应用无法直接使用。
# 必须把网卡从内核驱动"抢过来"，绑定到 DPDK 的用户态驱动（uio_pci_generic 或 vfio-pci）。
#
# 绑定后：
#   内核视角：ens192 网卡消失（或显示为 "unmanaged"）
#   DPDK 视角：0000:0b:00.0 可以被 DPDK EAL 扫描到并使用
# ==============================================

set -euo pipefail
# 加载公共函数库
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 需要 root 权限（绑定网卡、加载驱动需要）
require_root_for_write
# 检查 DPDK_PCI 不能与管理 PCI 相同（防止误操作导致 SSH 断开）
guard_not_mgmt_pci

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/PREPARE_VMXNET3.txt"
: > "${OUT}"
# 记录执行的命令
append_command_log "${RECORD_DIR}" "sudo" "$0"

# 查找 dpdk-devbind.py 工具
if ! devbind="$(find_devbind 2>/dev/null)"; then
    echo "ERROR: dpdk-devbind not found" | tee -a "${OUT}"
    exit 1
fi

{
    echo "# PREPARE_VMXNET3"
    echo
    echo "## env"
    # 打印项目环境变量（DPDK_IF、DPDK_PCI、DPDK_DRIVER 等）
    print_project_env
    echo
    echo "## setup hugepages"
    # 配置大页内存（DPDK 应用需要）
    setup_hugepages
    # 显示大页状态
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
    mount | grep hugetlbfs || true
    echo
    echo "## load driver"
    # 根据配置的 DPDK_DRIVER 加载对应的内核模块
    if [[ "${DPDK_DRIVER}" == "uio_pci_generic" ]]; then
        # uio_pci_generic：老的 DPDK 用户态驱动，通用性好
        modprobe uio || true
        modprobe uio_pci_generic || true
    elif [[ "${DPDK_DRIVER}" == "vfio-pci" ]]; then
        # vfio-pci：新的 DPDK 用户态驱动，需要 IOMMU 支持
        modprobe vfio-pci || true
    else
        # 其他驱动（如 igb_uio）
        modprobe "${DPDK_DRIVER}" || true
    fi
    # 确认驱动模块已加载
    lsmod | grep -E 'uio|vfio' || true
    echo
    echo "## before bind"
    # 显示绑定前的网卡状态（哪些网卡用了哪些驱动）
    "${devbind}" --status || true
    echo
    echo "## bring DPDK interface down if exists"
    # 如果 DPDK 网卡当前处于 up 状态，先 down 掉
    # （内核驱动管理的网卡必须 down 才能解绑）
    ip link set "${DPDK_IF}" down 2>/dev/null || true
    echo
    echo "## bind ${DPDK_PCI} to ${DPDK_DRIVER}"
    # 核心操作：将 PCI 设备绑定到 DPDK 驱动
    # 执行后，内核驱动（vmxnet3）失去对网卡的控制
    # DPDK 驱动（uio_pci_generic/vfio-pci）接管网卡
    echo "command: ${devbind} --bind=${DPDK_DRIVER} ${DPDK_PCI}"
    "${devbind}" --bind="${DPDK_DRIVER}" "${DPDK_PCI}"
    echo
    echo "## after bind"
    # 显示绑定后的网卡状态
    "${devbind}" --status || true
} >> "${OUT}" 2>&1

echo "[OK] prepare saved: ${OUT}"
