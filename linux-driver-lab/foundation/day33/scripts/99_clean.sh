#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rm -rf "${WORKDIR}/rootfs"/* "${WORKDIR}/runs/${RUN_ID}"        "${WORKDIR}/tools/aarch64/day33_edu_trace_tool"        "${ROOTFS_IMG}"
rm -f "${DAY33_ROOT}/driver"/*.o "${DAY33_ROOT}/driver"/*.ko "${DAY33_ROOT}/driver"/*.mod* "${DAY33_ROOT}/driver"/*.order "${DAY33_ROOT}/driver"/*.symvers
find "${DAY33_ROOT}/driver" -maxdepth 1 -type d -name '.tmp_versions' -exec rm -rf {} +
echo '[day33] clean 完成'
