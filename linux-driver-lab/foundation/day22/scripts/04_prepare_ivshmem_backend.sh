#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

require_cmd "${TRUNCATE_BIN}"
RUN_DIR="${RUNS_DIR}/${RUN_ID}"
ensure_dir "${RUN_DIR}"

BACKEND_FILE="${RUN_DIR}/${IVSHMEM_BACKEND_FILENAME}"
BACKEND_LOG="${RUN_DIR}/ivshmem-backend.log"

rm -f "${BACKEND_FILE}"
${TRUNCATE_BIN} -s "${IVSHMEM_SIZE}" "${BACKEND_FILE}"

{
    echo "# day22 ivshmem plain backend"
    echo "run-id=${RUN_ID}"
    echo "backend-file=${BACKEND_FILE}"
    echo "size=${IVSHMEM_SIZE}"
} > "${BACKEND_LOG}"

log "ivshmem plain backend 文件已准备：${BACKEND_FILE}"
