#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rm -rf "${WORKDIR}"
rm -f "${DAY26_ROOT}/driver"/*.o "${DAY26_ROOT}/driver"/*.ko "${DAY26_ROOT}/driver"/*.mod \
      "${DAY26_ROOT}/driver"/*.mod.c "${DAY26_ROOT}/driver"/*.mod.o \
      "${DAY26_ROOT}/driver"/Module.symvers "${DAY26_ROOT}/driver"/modules.order
find "${DAY26_ROOT}/driver" -maxdepth 1 -type f -name '.*.cmd' -delete || true
