#!/usr/bin/env bash
#===============================================================================
# 02_prepare_vmxnet3.sh - 准备 vmxnet3 网卡环境
# 作用：解绑网卡、加载 UIO 驱动、绑定 vmxnet3 驱动、配置大页
# 说明：调用上游 project-user-space-fastpath 的同名脚本
#===============================================================================
set -euo pipefail

# 引入公共变量和函数
source "$(dirname "$0")/common.sh"

# 环境准备日志输出文件
OUT="${RECORD_DIR}/PREPARE_VMXNET3.txt"

{
  echo "# PREPARE_VMXNET3"
  echo
  log_env                  # 输出环境变量（便于复现）
  echo

  #----------------------------------------
  # 调用上游 fastpath 的 prepare 脚本
  # 上游脚本处理：驱动解绑 → 绑定 uio_pci_generic → 大页配置
  #----------------------------------------
  if [[ -x "${UPSTREAM_FASTPATH_DIR}/scripts/02_prepare_vmxnet3.sh" ]]; then
    echo "## reuse upstream fastpath prepare script"
    cd "${UPSTREAM_FASTPATH_DIR}"
    # 传递 DPDK 相关环境变量给上游脚本
    DPDK_IF="${DPDK_IF}" DPDK_PCI="${DPDK_PCI}" DPDK_DRIVER="${DPDK_DRIVER}" \
    MGMT_IF="${MGMT_IF}" MGMT_PCI="${MGMT_PCI}" HUGEPAGES="${HUGEPAGES}" \
      ./scripts/02_prepare_vmxnet3.sh
  else
    echo "[ERR] upstream prepare script missing"
    exit 1
  fi
} 2>&1 | tee "${OUT}"

echo "[OK] prepare saved: ${OUT}"