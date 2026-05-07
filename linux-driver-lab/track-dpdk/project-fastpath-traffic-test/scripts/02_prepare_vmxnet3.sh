#!/usr/env bash
#===============================================================================
# 02_prepare_vmxnet3.sh - 准备 vmxnet3 DPDK 端口
# 作用：预留 hugepage、解除原 driver 绑定、挂载 uio_pci_generic
# 输出：records/<tag>/PREPARE_VMXNET3.txt
#===============================================================================
source "$(dirname "$0")/common.sh"

OUT="${RECORD_DIR}/PREPARE_VMXNET3.txt"
{
  echo "# PREPARE_VMXNET3"
  echo
  log_env
  echo
  echo "## call upstream prepare script"
  cd "${FASTPATH_PROJECT_DIR}"
  DPDK_IF="${DPDK_IF}" DPDK_PCI="${DPDK_PCI}" DPDK_DRIVER="${DPDK_DRIVER}" MGMT_IF="${MGMT_IF}" MGMT_PCI="${MGMT_PCI}" HUGEPAGES="${HUGEPAGES}" \
    ./scripts/02_prepare_vmxnet3.sh
} | tee "${OUT}"

echo "[OK] prepare saved: ${OUT}"
