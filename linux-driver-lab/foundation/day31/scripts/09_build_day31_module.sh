#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# Day31 run 流程默认总是重编模块，避免源码已更新但旧 ko 残留。
echo '[day31] 开始构建 day31 bench 驱动模块'
echo "[day31] ARCH=${ARCH}"
echo "[day31] CROSS_COMPILE=${CROSS_COMPILE}"
require_file "${KDIR}" KDIR
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc
make -C "${DAY31_ROOT}/driver" clean || true
make -C "${DAY31_ROOT}/driver"   KDIR="${KDIR}"   ARCH="${ARCH}"   CROSS_COMPILE="${CROSS_COMPILE}"
require_file "${DAY31_ROOT}/driver/day31_edu_bench.ko" day31_edu_bench.ko
ls -l "${DAY31_ROOT}/driver/day31_edu_bench.ko"
