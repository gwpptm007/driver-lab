#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

require_exec "${QEMU_BIN}" QEMU_BIN
require_exec "${HOST_CC}" HOST_CC
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc
require_exec "${CROSS_COMPILE}strip" CROSS_COMPILE-strip
require_file "${KDIR}" KDIR
require_file "${KERNEL_IMAGE}" KERNEL_IMAGE
require_file "${KERNEL_CONFIG_PATH}" KERNEL_CONFIG_PATH
require_exec "${BUSYBOX_BIN}" BUSYBOX_BIN

echo "[day34] 检查内核 PCI/MSI 关键配置：${KERNEL_CONFIG_PATH}"
grep -q '^CONFIG_PCI=y' "${KERNEL_CONFIG_PATH}" && echo '[OK] CONFIG_PCI=y' || { echo '[ERROR] missing CONFIG_PCI=y'; exit 1; }
grep -q '^CONFIG_PCI_MSI=y' "${KERNEL_CONFIG_PATH}" && echo '[OK] CONFIG_PCI_MSI=y' || { echo '[ERROR] missing CONFIG_PCI_MSI=y'; exit 1; }
grep -q '^CONFIG_PCI_HOST_GENERIC=y' "${KERNEL_CONFIG_PATH}" && echo '[OK] CONFIG_PCI_HOST_GENERIC=y' || { echo '[ERROR] missing CONFIG_PCI_HOST_GENERIC=y'; exit 1; }
