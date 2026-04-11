#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day30] 开始构建 day30 mmap zero-copy 驱动模块'
echo "[day30] ARCH=${ARCH}"
echo "[day30] CROSS_COMPILE=${CROSS_COMPILE}"
require_file "${KDIR}" KDIR
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc
make -C "${DAY30_ROOT}/driver" clean || true
make -C "${DAY30_ROOT}/driver" \
  KDIR="${KDIR}" \
  ARCH="${ARCH}" \
  CROSS_COMPILE="${CROSS_COMPILE}"
require_file "${DAY30_ROOT}/driver/day30_edu_mmap.ko" day30_edu_mmap.ko
ls -l "${DAY30_ROOT}/driver/day30_edu_mmap.ko"
