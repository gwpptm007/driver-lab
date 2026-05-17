#!/usr/bin/env bash
# 脚本: 01_setup_hugepages.sh
# 功能: 配置大页内存（hugepages），为 DPDK vhost-user 提供共享内存
# 用法: sudo ./scripts/01_setup_hugepages.sh
#
# ========== 大页内存原理 ==========
# Linux 大页（hugepages）是一种预分配的物理内存页面，比普通 4KB 页大得多。
# DPDK 使用大页可以减少 TLB miss、提升内存访问效率、避免内存碎片。
#
# 关键概念：
#   Hugepagesize  每个大页的大小，在 x86_64 上通常为 2MB (2048 kB)
#   HUGEPAGES     要分配的大页数量
#   总内存 = Hugepagesize × HUGEPAGES
#
# 示例（设置 2GB 大页）:
#   Hugepagesize = 2048 kB = 2 MB
#   HUGEPAGES    = 1024
#   总内存       = 1024 × 2 MB = 2048 MB = 2 GB
#
# 查看当前大页状态：
#   grep Huge /proc/meminfo   # 显示 HugePages_Total/Free/Hugepagesize
#   mount | grep hugetlbfs    # 查看 hugetlbfs 挂载点
#   df -h /mnt/huge           # 查看大页文件系统可用空间
#
# 配置文件（重启后失效，需每次启动时重新配置）：
#   /proc/sys/vm/nr_hugepages   # 写入大页数量
#   /mnt/huge                   # 大页挂载点（hugetlbfs）
#
# ==============================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 需要 root 权限（大页配置需修改系统参数）
require_root_for_write

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/HUGEPAGE_SETUP.txt"
: > "${OUT}"

# 记录配置前的大页状态
{
    echo "# HUGEPAGE_SETUP"
    echo "date=$(date '+%F %T')"
    echo
    echo "## Before"
    grep Huge /proc/meminfo || true
    mount | grep -E "hugetlbfs|${HUGEPAGE_MOUNT}" || true
    echo
    echo "## Apply"
    echo "HUGEPAGES=${HUGEPAGES}      # 大页数量（2MB × 1024 = 2GB）"
    echo "HUGEPAGE_MOUNT=${HUGEPAGE_MOUNT}  # hugetlbfs 挂载点"
} >> "${OUT}" 2>&1

# Step 1: 创建大页挂载目录
mkdir -p "${HUGEPAGE_MOUNT}"

# Step 2: 设置大页数量（写入 /proc/sys/vm/nr_hugepages）
# 此操作分配 HUGEPAGES × Hugepagesize 的物理内存
echo "${HUGEPAGES}" > /proc/sys/vm/nr_hugepages

# Step 3: 挂载 hugetlbfs 文件系统（如果尚未挂载）
# DPDK 应用通过此挂载点访问大页内存
if ! mountpoint -q "${HUGEPAGE_MOUNT}"; then
    mount -t hugetlbfs nodev "${HUGEPAGE_MOUNT}"
fi

# 记录配置后的大页状态
{
    echo
    echo "## After"
    grep Huge /proc/meminfo || true
    mount | grep -E "hugetlbfs|${HUGEPAGE_MOUNT}" || true
    df -h "${HUGEPAGE_MOUNT}" || true
} >> "${OUT}" 2>&1

cat <<EOF_OUT
[OK] Hugepage setup saved:
${OUT}

Next:
  sudo ./scripts/02_run_vhost_testpmd.sh
EOF_OUT
