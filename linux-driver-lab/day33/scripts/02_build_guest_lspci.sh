#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

if [ -x "${GUEST_LSPCI_BIN}" ]; then
  echo "[day33] 已存在可执行 guest lspci：${GUEST_LSPCI_BIN}"
  exit 0
fi
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc
require_exec make make
require_file "${PCIUTILS_SRC_DIR}/Makefile" PCIUTILS_SRC_DIR/Makefile

echo '[day33] 开始构建 guest 侧 arm64 静态 lspci'
make -C "${PCIUTILS_SRC_DIR}" clean >/dev/null 2>&1 || true
make -C "${PCIUTILS_SRC_DIR}"   CC="${CROSS_COMPILE}gcc"   STRIP="${CROSS_COMPILE}strip"   HOST="${CROSS_COMPILE%?}"   ZLIB=no DNS=no SHARED=no IDSDIR= LIBKMOD=no PCI_IDS=   CFLAGS='-O2 -static' LDFLAGS='-static' lspci
require_exec "${GUEST_LSPCI_BIN}" GUEST_LSPCI_BIN
echo "[day33] guest lspci 已生成：${GUEST_LSPCI_BIN}"
