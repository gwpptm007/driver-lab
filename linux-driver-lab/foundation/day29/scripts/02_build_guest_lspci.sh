#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day29] 开始构建 guest 侧 arm64 静态 lspci'
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
