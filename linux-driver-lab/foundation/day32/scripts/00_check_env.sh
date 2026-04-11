#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day32] 开始检查 day32 宿主机环境（perf 版本）'
require_exec "${QEMU_BIN}" QEMU_BIN
require_file "${KERNEL_IMAGE}" KERNEL_IMAGE
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
if [ -x "${GUEST_LSPCI_BIN}" ]; then
  echo "[day32] 发现可直接使用的 arm64 静态 lspci：${GUEST_LSPCI_BIN}"
else
  echo "[day32][WARN] 当前没有可执行的 GUEST_LSPCI_BIN：${GUEST_LSPCI_BIN}"
  echo '[day32][WARN] 后续会尝试从 PCIUTILS_SRC_DIR 构建。'
fi
if command -v perf >/dev/null 2>&1; then
  echo "[day32] 发现宿主 perf：$(command -v perf)"
else
  echo '[day32][WARN] 宿主未发现 perf；默认 full 流程不受影响，但 perf-baseline/perf-optimized 需要 perf。'
fi
if [ -n "${KERNEL_CONFIG_PATH:-}" ]; then
  require_file "${KERNEL_CONFIG_PATH}" KERNEL_CONFIG_PATH
fi
cat <<EOI
DAY32_ROOT              : ${DAY32_ROOT}
WORKDIR                 : ${WORKDIR}
QEMU_BIN                : ${QEMU_BIN}
KERNEL_IMAGE            : ${KERNEL_IMAGE}
BUSYBOX_BIN             : ${BUSYBOX_BIN}
GUEST_LSPCI_BIN         : ${GUEST_LSPCI_BIN}
KERNEL_CONFIG_PATH      : ${KERNEL_CONFIG_PATH:-<unset>}
EDU_DEVICE_ID_EXPECT    : ${EDU_DEVICE_ID_EXPECT}
EDU_QEMU_DMA_MASK       : ${EDU_QEMU_DMA_MASK}
DAY32_VERIFY_LEN        : ${DAY32_VERIFY_LEN}
DAY32_VERIFY_SEED       : ${DAY32_VERIFY_SEED}
DAY32_PERF_LEN          : ${DAY32_PERF_LEN}
DAY32_PERF_ITER         : ${DAY32_PERF_ITER}
DAY32_PERF_WARMUP       : ${DAY32_PERF_WARMUP}
DAY32_DMA_LITE_LEN      : ${DAY32_DMA_LITE_LEN}
DAY32_DMA_LITE_ITER     : ${DAY32_DMA_LITE_ITER}
DAY32_DMA_LITE_WARMUP   : ${DAY32_DMA_LITE_WARMUP}
DAY32_PROFILE_MODE      : ${DAY32_PROFILE_MODE}
DAY32_RUN_DMA_LITE      : ${DAY32_RUN_DMA_LITE}
QEMU_TIMEOUT_SEC        : ${QEMU_TIMEOUT_SEC}
EOI

if [ -n "${KERNEL_CONFIG_PATH:-}" ]; then
  echo "[day32] 检查内核 PCI/MSI 关键配置：${KERNEL_CONFIG_PATH}"
  for k in CONFIG_PCI CONFIG_PCI_MSI CONFIG_PCI_HOST_GENERIC; do
    if grep -q "^${k}=y" "${KERNEL_CONFIG_PATH}"; then
      echo "[OK] ${k}=y"
    else
      echo "[WARN] ${k} not set to y"
    fi
  done
fi
