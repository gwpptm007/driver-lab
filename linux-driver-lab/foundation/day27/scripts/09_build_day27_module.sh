#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 构建 Day27 的外部模块。这里必须走 day27 顶层入口，而不是进入 driver 目录手工 make，
# 这样才能保证 KDIR / ARCH / CROSS_COMPILE 与项目里前面几天保持一致。

echo '[day27] 开始构建 day27 循环稳定性驱动模块'
echo "[day27] ARCH=${ARCH}"
echo "[day27] CROSS_COMPILE=${CROSS_COMPILE}"
echo "[day27] KDIR=${KDIR}"
require_file "${KDIR}" KDIR
make -C "${DAY27_ROOT}/driver" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" KDIR="${KDIR}"
require_file "${DAY27_ROOT}/driver/day27_edu_loop.ko" day27_edu_loop.ko
echo "[day27] 模块已生成：${DAY27_ROOT}/driver/day27_edu_loop.ko"
