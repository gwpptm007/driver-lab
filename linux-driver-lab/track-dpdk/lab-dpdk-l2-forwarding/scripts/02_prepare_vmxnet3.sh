#!/usr/bin/env bash
# =============================================================================
# 02_prepare_vmxnet3.sh — 准备 VMXNET3 网卡供 DPDK 使用（需要 root）
#
# 这个脚本会修改系统状态：
#   1. 挂载 hugetlbfs 大页文件系统
#   2. 分配 hugepage（默认 1024 × 2MB = 2GB）
#   3. 加载 uio / uio_pci_generic 内核模块
#   4. 将 DPDK 网卡（ens192 / 0000:0b:00.0）从内核驱动绑定到 DPDK 用户态驱动
#
# 注意：绑定后 ens192 接口会从系统中消失（不再出现在 ip addr 中），
#       因为它的 PCI 设备已经被 DPDK 接管。
#
# 产出：records/<timestamp>-dpdk-l2-forwarding/PREPARE_VMXNET3.txt
# =============================================================================
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 需要 root 权限（写 /proc、modprobe、dpdk-devbind 都需要）
require_root_for_write
# 安全检查：不能把管理网卡绑走
guard_not_mgmt_pci

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/PREPARE_VMXNET3.txt"
: > "${OUT}"
append_command_log "${RECORD_DIR}" "sudo" "$0"

# 查找 dpdk-devbind 工具
if ! devbind="$(find_devbind 2>/dev/null)"; then
    echo "ERROR: dpdk-devbind not found" | tee -a "${OUT}"
    exit 1
fi

{
    echo "# PREPARE_VMXNET3"
    echo
    # ── 打印环境变量 ─────────────────────────────────────────
    echo "## env"
    print_lab_env
    echo
    # ── 挂载 hugepage 文件系统 ────────────────────────────────
    # DPDK 需要通过 hugetlbfs 使用大页内存
    echo "## setup hugepages"
    mkdir -p "${HUGEPAGE_MOUNT}"
    if ! mountpoint -q "${HUGEPAGE_MOUNT}"; then
        mount -t hugetlbfs nodev "${HUGEPAGE_MOUNT}"
    fi
    # 写入期望的 hugepage 数量
    echo "${HUGEPAGES}" > /proc/sys/vm/nr_hugepages
    # 验证分配结果
    grep -E 'HugePages|Hugepagesize|Hugetlb' /proc/meminfo || true
    echo
    # ── 加载用户态驱动模块 ────────────────────────────────────
    # uio_pci_generic: 通用 UIO 驱动，将 PCI 设备暴露给用户态
    # 替代方案：vfio-pci（支持 IOMMU，更安全但配置更复杂）
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
    # ── 绑定前状态 ───────────────────────────────────────────
    echo "## before bind"
    "${devbind}" --status || true
    echo
    # ── 关闭网卡接口 ─────────────────────────────────────────
    # 绑定前需要先 down 掉接口，否则可能失败
    echo "## bring interface down if exists"
    ip link set "${DPDK_IF}" down 2>/dev/null || true
    echo
    # ── 执行绑定 ─────────────────────────────────────────────
    # 将 PCI 设备从内核驱动（vmxnet3）切换到 uio_pci_generic
    # 绑定后，内核不再管理这个设备，DPDK 应用可以通过用户态直接访问
    echo "## bind ${DPDK_PCI} to ${DPDK_DRIVER}"
    "${devbind}" --bind="${DPDK_DRIVER}" "${DPDK_PCI}"
    echo
    # ── 绑定后状态 ───────────────────────────────────────────
    echo "## after bind"
    "${devbind}" --status || true
} >> "${OUT}" 2>&1

echo "[OK] prepare saved: ${OUT}"
