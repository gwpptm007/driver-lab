#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# Day25 模块构建入口。
# 这里显式打印 ARCH / CROSS_COMPILE / KDIR，避免再次出现
# “宿主机 x86 默认规则 + aarch64 编译器”这种混搭问题。

echo "[day25] 开始构建 day25 PCI probe 模块"
echo "[day25] ARCH=${ARCH}"
echo "[day25] CROSS_COMPILE=${CROSS_COMPILE}"
echo "[day25] KDIR=${KDIR}"
require_file "${KDIR}" KDIR
make -C "${DAY25_ROOT}/driver" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" KDIR="${KDIR}"
echo "[day25] 模块已生成：${DAY25_ROOT}/driver/day25_edu_irq.ko"
