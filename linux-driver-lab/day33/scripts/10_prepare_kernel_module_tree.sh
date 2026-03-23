#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_file "${KDIR}/Makefile" KDIR/Makefile
ensure_dir "${DAY33_ROOT}/driver/include"
echo "[day33] 已确认外部模块构建树：${KDIR}"
