#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day32] 开始构建 day32 perf 驱动模块'
echo "[day32] ARCH=${ARCH}"
echo "[day32] CROSS_COMPILE=${CROSS_COMPILE}"
require_file "${KDIR}" KDIR
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc
make -C "${DAY32_ROOT}/driver" clean || true
make -C "${DAY32_ROOT}/driver" KDIR="${KDIR}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}"
require_file "${DAY32_ROOT}/driver/day32_edu_perf.ko" day32_edu_perf.ko
ls -l "${DAY32_ROOT}/driver/day32_edu_perf.ko"
