#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# day31 guest 里只需要 lspci，不需要 pciutils 全家桶；
# 因此这里专门静态构建一个最小 lspci 供 initramfs 复用。
echo '[day31] 开始构建 guest 侧 arm64 静态 lspci'
require_file "${PCIUTILS_SRC_DIR}" PCIUTILS_SRC_DIR
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc

chmod +x "${PCIUTILS_SRC_DIR}/lib/configure" 2>/dev/null || true
chmod +x "${PCIUTILS_SRC_DIR}/configure" 2>/dev/null || true

make -C "${PCIUTILS_SRC_DIR}" clean || true
make -C "${PCIUTILS_SRC_DIR}" \
  HOST=aarch64-linux-gnu \
  CROSS_COMPILE="${CROSS_COMPILE}" \
  DNS=no ZLIB=no SHARED=no \
  LDFLAGS="-static" \
  lspci

require_exec "${PCIUTILS_SRC_DIR}/lspci" lspci
file "${PCIUTILS_SRC_DIR}/lspci" || true
