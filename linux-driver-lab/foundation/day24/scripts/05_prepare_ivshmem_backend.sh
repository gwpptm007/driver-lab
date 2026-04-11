#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
ensure_run_dir

mkdir -p "$(dirname "${IVSHMEM_BACKEND_FILE}")"
truncate -s "${IVSHMEM_SIZE}" "${IVSHMEM_BACKEND_FILE}"
log "ivshmem plain backend 文件已准备：${IVSHMEM_BACKEND_FILE}"
