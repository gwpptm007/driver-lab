#!/usr/bin/env bash
# 脚本: 01_setup_hugepages.sh
# 功能: 配置大页内存（hugepages），用于 DPDK 共享内存
# 用法: sudo ./scripts/01_setup_hugepages.sh

set -euo pipefail
# 加载公共函数库
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
# 需要 root 权限（修改系统大页配置）
require_root_for_write

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/HUGEPAGE_SETUP.txt"
: > "${OUT}"

{
    echo "# HUGEPAGE_SETUP"
    echo "date=$(date '+%F %T')"
    echo "HUGEPAGES=${HUGEPAGES}"
    echo "HUGEPAGE_MOUNT=${HUGEPAGE_MOUNT}"
    echo
    echo "## Before"
    grep Huge /proc/meminfo || true
    mount | grep hugetlbfs || true
    echo
} >> "${OUT}"

# 创建大页挂载目录
mkdir -p "${HUGEPAGE_MOUNT}"

# 如果尚未挂载 hugetlbfs，则挂载
if ! mount | grep -q " ${HUGEPAGE_MOUNT} "; then
    mount -t hugetlbfs nodev "${HUGEPAGE_MOUNT}"
fi

# 设置大页数量（写入 /proc/sys/vm/nr_hugepages）
echo "${HUGEPAGES}" > /proc/sys/vm/nr_hugepages

{
    echo
    echo "## After"
    grep Huge /proc/meminfo || true
    mount | grep hugetlbfs || true
} >> "${OUT}" 2>&1

cat <<EOF

[OK] Hugepage setup saved:
${OUT}

Next:
  ./scripts/02_bind_vmxnet3.sh status
EOF
