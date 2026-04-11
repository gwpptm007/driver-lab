#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[day26] 开始构建 day26 用户态工具友好驱动模块"
echo "[day26] ARCH=${ARCH}"
echo "[day26] CROSS_COMPILE=${CROSS_COMPILE}"
echo "[day26] KDIR=${KDIR}"
require_file "${KDIR}" KDIR
# 关键点：这里进入 day26/driver 目录，再由 driver/Makefile 把 M=$(CURDIR) 传给 Kbuild。
# 这样 .ko/.o/Module.symvers 都会稳定输出在 day26/driver/ 下。
make -C "${DAY26_ROOT}/driver" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" KDIR="${KDIR}"
echo "[day26] 模块已生成：${DAY26_ROOT}/driver/day26_edu_tool.ko"
