#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

require_file "${KDIR}/Makefile" KDIR/Makefile
require_exec "${CROSS_COMPILE}gcc" CROSS_COMPILE-gcc

echo '[day33] 开始构建 day33 ftrace 基线驱动模块'
echo "[day33] ARCH=${ARCH}"
echo "[day33] CROSS_COMPILE=${CROSS_COMPILE}"
make -C "${KDIR}" M="${DAY33_ROOT}/driver" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" modules
ls -l "${DAY33_ROOT}/driver/day33_edu_trace.ko"
