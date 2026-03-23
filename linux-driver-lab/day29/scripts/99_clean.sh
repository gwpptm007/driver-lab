#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rm -rf "${WORKDIR}"
rm -f "${DAY29_ROOT}/driver"/*.ko "${DAY29_ROOT}/driver"/*.o "${DAY29_ROOT}/driver"/*.mod.c \
      "${DAY29_ROOT}/driver"/*.mod "${DAY29_ROOT}/driver"/*.symvers "${DAY29_ROOT}/driver"/modules.order
rm -rf "${DAY29_ROOT}/driver"/.tmp_versions
echo '[day29] workdir 与 day29 临时构建产物已清理'
