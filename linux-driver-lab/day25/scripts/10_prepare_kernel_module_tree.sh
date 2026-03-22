#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 为外部模块构建准备内核模块树。
# 关键点：如果 Module.symvers 里缺少 __pci_register_driver 等 PCI 导出符号，
# 就必须继续执行 make modules，而不能只停在 modules_prepare。
require_file "${KERNEL_SRC_ROOT:-}" KERNEL_SRC_ROOT
require_file "${KDIR:-}" KDIR

echo "[day25] 准备 arm64 内核模块树"
make -C "${KERNEL_SRC_ROOT}"   ARCH="${ARCH}"   CROSS_COMPILE="${CROSS_COMPILE}"   O="${KDIR}"   modules_prepare

if [ ! -f "${KDIR}/Module.symvers" ] || ! grep -q '__pci_register_driver' "${KDIR}/Module.symvers"; then
  echo "[day25] Module.symvers 缺少 PCI 符号，继续执行 make modules"
  make -C "${KERNEL_SRC_ROOT}"     -j"$(nproc)"     ARCH="${ARCH}"     CROSS_COMPILE="${CROSS_COMPILE}"     O="${KDIR}"     modules
fi

echo "[day25] 内核模块树准备完成：${KDIR}"
