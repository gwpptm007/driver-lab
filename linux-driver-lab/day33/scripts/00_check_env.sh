#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day33] 开始检查 day33 宿主机环境（ftrace/function_graph）'
require_exec "${QEMU_BIN}" QEMU_BIN
require_file "${KERNEL_IMAGE}" KERNEL_IMAGE
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
if [ -x "${GUEST_LSPCI_BIN}" ]; then
  echo "[day33] 发现可直接使用的 arm64 静态 lspci：${GUEST_LSPCI_BIN}"
else
  echo "[day33][WARN] 当前没有可执行的 GUEST_LSPCI_BIN：${GUEST_LSPCI_BIN}"
  echo '[day33][WARN] 后续会尝试从 PCIUTILS_SRC_DIR 构建。'
fi
if [ -n "${KERNEL_CONFIG_PATH:-}" ]; then
  require_file "${KERNEL_CONFIG_PATH}" KERNEL_CONFIG_PATH
fi
cat <<EOI
DAY33_ROOT              : ${DAY33_ROOT}
WORKDIR                 : ${WORKDIR}
QEMU_BIN                : ${QEMU_BIN}
KERNEL_IMAGE            : ${KERNEL_IMAGE}
BUSYBOX_BIN             : ${BUSYBOX_BIN}
GUEST_LSPCI_BIN         : ${GUEST_LSPCI_BIN}
KERNEL_CONFIG_PATH      : ${KERNEL_CONFIG_PATH:-<unset>}
EDU_DEVICE_ID_EXPECT    : ${EDU_DEVICE_ID_EXPECT}
EDU_QEMU_DMA_MASK       : ${EDU_QEMU_DMA_MASK}
DAY33_VERIFY_LEN        : ${DAY33_VERIFY_LEN}
DAY33_VERIFY_SEED       : ${DAY33_VERIFY_SEED}
DAY33_TRACE_LEN         : ${DAY33_TRACE_LEN}
DAY33_TRACE_SEED        : ${DAY33_TRACE_SEED}
DAY33_TRACE_WORKLOAD    : ${DAY33_TRACE_WORKLOAD}
DAY33_TRACE_DMA_ITER    : ${DAY33_TRACE_DMA_ITER}
DAY33_TRACE_DMA_WARMUP  : ${DAY33_TRACE_DMA_WARMUP}
QEMU_TIMEOUT_SEC        : ${QEMU_TIMEOUT_SEC}
EOI

if [ -n "${KERNEL_CONFIG_PATH:-}" ]; then
  echo "[day33] 检查内核 PCI/MSI 与 tracing 关键配置：${KERNEL_CONFIG_PATH}"
  for k in CONFIG_PCI CONFIG_PCI_MSI CONFIG_PCI_HOST_GENERIC CONFIG_TRACING CONFIG_FTRACE CONFIG_FUNCTION_TRACER CONFIG_FUNCTION_GRAPH_TRACER CONFIG_DEBUG_FS; do
    if grep -q "^${k}=y" "${KERNEL_CONFIG_PATH}"; then
      echo "[OK] ${k}=y"
    else
      echo "[WARN] ${k} not set to y"
    fi
  done
fi
