#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

require_file "${KERNEL_SRC_ROOT:-}" KERNEL_SRC_ROOT
require_file "${KDIR:-}" KDIR

echo "[day26] 准备 arm64 内核模块树"
make -C "${KERNEL_SRC_ROOT}" \
  ARCH="${ARCH}" \
  CROSS_COMPILE="${CROSS_COMPILE}" \
  O="${KDIR}" \
  modules_prepare

# 如果 Module.symvers 缺少 PCI 导出符号，则继续补一轮 make modules。
if [ ! -f "${KDIR}/Module.symvers" ] || ! grep -q '__pci_register_driver' "${KDIR}/Module.symvers"; then
  echo "[day26] Module.symvers 缺少 PCI 符号，继续执行 make modules"
  make -C "${KERNEL_SRC_ROOT}" \
    -j"$(nproc)" \
    ARCH="${ARCH}" \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    O="${KDIR}" \
    modules
fi

echo "[day26] 内核模块树准备完成：${KDIR}"
