#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rm -rf "${WORKDIR}"
rm -f "${DAY30_ROOT}/driver"/*.ko "${DAY30_ROOT}/driver"/*.o "${DAY30_ROOT}/driver"/*.mod.c \
      "${DAY30_ROOT}/driver"/*.mod "${DAY30_ROOT}/driver"/*.symvers "${DAY30_ROOT}/driver"/modules.order
rm -rf "${DAY30_ROOT}/driver"/.tmp_versions
echo '[day30] workdir 与 day30 临时构建产物已清理'
