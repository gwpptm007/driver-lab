#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day34] 开始构建 guest 侧 arm64 静态 lspci'
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc
require_exec "${CROSS_COMPILE}strip" CROSS_COMPILE-strip
require_file "${PCIUTILS_SRC_DIR}" PCIUTILS_SRC_DIR
(
  cd "${PCIUTILS_SRC_DIR}"
  make clean >/dev/null 2>&1 || true
  #
  # day34 的 initramfs 里没有动态链接器，因此这里显式要求静态链接，
  # 避免 guest 中出现“/bin/lspci: not found”（其实是解释器缺失）的假象。
  #
  make CC="${CROSS_COMPILE}gcc" STRIP="${CROSS_COMPILE}strip" HOST="${CROSS_COMPILE%?}"        ZLIB=no DNS=no SHARED=no IDSDIR= PREFIX=        CFLAGS="-O2 -static" LDFLAGS="-static" all
)
require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
echo "[day34] guest lspci 已生成：${GUEST_LSPCI_BIN}"
