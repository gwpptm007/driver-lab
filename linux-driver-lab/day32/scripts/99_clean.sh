#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rm -rf "${WORKDIR}"
rm -f "${DAY32_ROOT}/driver"/*.ko "${DAY32_ROOT}/driver"/*.o "${DAY32_ROOT}/driver"/*.mod.c       "${DAY32_ROOT}/driver"/*.mod "${DAY32_ROOT}/driver"/*.symvers "${DAY32_ROOT}/driver"/modules.order
rm -rf "${DAY32_ROOT}/driver"/.tmp_versions
echo '[day32] workdir 与 day32 临时构建产物已清理'
