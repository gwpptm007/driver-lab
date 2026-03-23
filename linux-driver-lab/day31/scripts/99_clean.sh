#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rm -rf "${WORKDIR}"
rm -f "${DAY31_ROOT}/driver"/*.ko "${DAY31_ROOT}/driver"/*.o "${DAY31_ROOT}/driver"/*.mod.c       "${DAY31_ROOT}/driver"/*.mod "${DAY31_ROOT}/driver"/*.symvers "${DAY31_ROOT}/driver"/modules.order
rm -rf "${DAY31_ROOT}/driver"/.tmp_versions
echo '[day31] workdir 与 day31 临时构建产物已清理'
