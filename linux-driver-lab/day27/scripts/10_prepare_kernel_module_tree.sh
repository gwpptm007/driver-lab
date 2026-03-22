#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 外部模块构建前，先把内核 build tree 准备好。
# modules_prepare 解决大部分情况；如果 Module.symvers 里还缺 PCI 符号，再自动补一轮 make modules。

echo '[day27] 准备 arm64 内核模块树'
require_file "${KERNEL_SRC_ROOT}" KERNEL_SRC_ROOT
require_file "${KDIR}" KDIR
make -C "${KERNEL_SRC_ROOT}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" O="${KDIR}" modules_prepare
if ! grep -q '__pci_register_driver' "${KDIR}/Module.symvers" 2>/dev/null; then
  echo '[day27][WARN] Module.symvers 中未发现 PCI 关键符号，补一轮 make modules。'
  make -C "${KERNEL_SRC_ROOT}" -j"$(nproc)" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" O="${KDIR}" modules
fi
echo "[day27] 内核模块树准备完成：${KDIR}"
