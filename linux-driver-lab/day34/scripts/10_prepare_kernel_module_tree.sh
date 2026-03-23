#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo '[day34] 检查 KDIR 外部模块构建树'
require_file "${KDIR}/Makefile" KDIR/Makefile
require_file "${KDIR}/Module.symvers" KDIR/Module.symvers
require_file "${KDIR}/.config" KDIR/.config
echo '[day34] kernel module tree looks ready'
