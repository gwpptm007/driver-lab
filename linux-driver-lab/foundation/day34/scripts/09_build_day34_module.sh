#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day34] 开始构建 day34 稳定性驱动模块'
require_file "${KDIR}" KDIR
make -C "${KDIR}" M="${DAY34_ROOT}/driver" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" modules
require_file "${DAY34_ROOT}/driver/day34_edu_stability.ko" day34_edu_stability.ko
ls -l "${DAY34_ROOT}/driver/day34_edu_stability.ko"
