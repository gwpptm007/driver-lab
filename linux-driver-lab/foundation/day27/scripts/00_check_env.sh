#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 这一步只检查“Day27 能不能开跑”，不做任何构建。
# 重点确认：
# 1. QEMU / Image / BusyBox / guest lspci 是否就绪；
# 2. 当前 arm64 .config 里是否真的打开了 PCI / MSI / host bridge；
# 3. 给出一份可读的环境摘要，便于后续排障。

echo '[day27] 开始检查 day27 宿主机环境（EDU loop 版本）'
require_exec "${QEMU_BIN}" QEMU_BIN
require_file "${KERNEL_IMAGE}" KERNEL_IMAGE
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN
if [ -x "${GUEST_LSPCI_BIN}" ]; then
  echo "[day27] 发现可直接使用的 arm64 静态 lspci：${GUEST_LSPCI_BIN}"
else
  echo "[day27][WARN] 当前没有可执行的 GUEST_LSPCI_BIN：${GUEST_LSPCI_BIN}"
  echo '[day27][WARN] 后续会尝试从 PCIUTILS_SRC_DIR 构建。'
fi
if [ -n "${KERNEL_CONFIG_PATH:-}" ]; then
  require_file "${KERNEL_CONFIG_PATH}" KERNEL_CONFIG_PATH
fi
cat <<EOF
DAY27_ROOT              : ${DAY27_ROOT}
WORKDIR                 : ${WORKDIR}
QEMU_BIN                : ${QEMU_BIN}
KERNEL_IMAGE            : ${KERNEL_IMAGE}
BUSYBOX_BIN             : ${BUSYBOX_BIN}
GUEST_LSPCI_BIN         : ${GUEST_LSPCI_BIN}
KERNEL_CONFIG_PATH      : ${KERNEL_CONFIG_PATH:-<unset>}
EDU_DEVICE_ID_EXPECT    : ${EDU_DEVICE_ID_EXPECT}
DAY27_LOOP_COUNT        : ${DAY27_LOOP_COUNT}
EOF

if [ -n "${KERNEL_CONFIG_PATH:-}" ]; then
  echo "[day27] 检查内核 PCI/MSI 关键配置：${KERNEL_CONFIG_PATH}"
  for k in CONFIG_PCI CONFIG_PCI_MSI CONFIG_PCI_HOST_GENERIC; do
    if grep -q "^${k}=y" "${KERNEL_CONFIG_PATH}"; then
      echo "[OK] ${k}=y"
    else
      echo "[WARN] ${k} not set to y"
    fi
  done
fi
