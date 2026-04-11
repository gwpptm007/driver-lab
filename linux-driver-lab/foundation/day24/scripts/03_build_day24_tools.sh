#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
ensure_run_dir

log "编译 day24 guest 侧用户态工具"
log "CC=${CROSS_COMPILE}gcc"
log "STRIP=${CROSS_COMPILE}strip"
require_cmd "${CROSS_COMPILE}gcc"
require_cmd "${CROSS_COMPILE}strip"

make -C "${DAY24_ROOT}/tools" \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    TOOL_OUT="${TOOL_FILE}" \
    all

require_file TOOL_FILE "${TOOL_FILE}"
chmod +x "${TOOL_FILE}" || true
file "${TOOL_FILE}" || true
log "guest 工具已生成：${TOOL_FILE}"
