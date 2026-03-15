#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

log "清理 day22 workdir 临时文件"
rm -rf "${WORKDIR}"
mkdir -p "${WORKDIR}"
log "已清理：${WORKDIR}"
