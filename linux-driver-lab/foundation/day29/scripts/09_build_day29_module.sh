#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day29] 开始构建 day29 coherent DMA 驱动模块'
echo "[day29] ARCH=${ARCH}"
echo "[day29] CROSS_COMPILE=${CROSS_COMPILE}"
# 这里每次先 clean 再 build，避免旧 .ko 残留让 rootfs 打进过期模块。
require_file "${KDIR}" KDIR
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc
make -C "${DAY29_ROOT}/driver" clean || true
make -C "${DAY29_ROOT}/driver" \
  KDIR="${KDIR}" \
  ARCH="${ARCH}" \
  CROSS_COMPILE="${CROSS_COMPILE}"
require_file "${DAY29_ROOT}/driver/day29_edu_dma.ko" day29_edu_dma.ko
ls -l "${DAY29_ROOT}/driver/day29_edu_dma.ko"
