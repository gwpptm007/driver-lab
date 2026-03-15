#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

require_cmd "${IVSHMEM_SERVER_BIN}"
ensure_dir "${RUNS_DIR}/${RUN_ID}"

RUN_DIR="${RUNS_DIR}/${RUN_ID}"
PIDFILE="${RUN_DIR}/ivshmem-server.pid"
SOCKET_PATH="${RUN_DIR}/ivshmem.sock"
SERVER_LOG="${RUN_DIR}/server.log"

# 如果 socket 或 pidfile 遗留，先清理，防止旧运行污染本次结果。
rm -f "${PIDFILE}" "${SOCKET_PATH}"

log "启动 ivshmem-server"
log "RUN_ID=${RUN_ID}"
log "SOCKET=${SOCKET_PATH}"
log "LOG=${SERVER_LOG}"

"${IVSHMEM_SERVER_BIN}" \
    -p "${PIDFILE}" \
    -S "${SOCKET_PATH}" \
    -m "${IVSHMEM_SHM_NAME}" \
    -l "${IVSHMEM_SIZE}" \
    -n "${IVSHMEM_VECTORS}" \
    >"${SERVER_LOG}" 2>&1 &

# 等待 socket 出现，避免后面 QEMU 抢跑。
for _ in $(seq 1 50); do
    if [[ -S "${SOCKET_PATH}" ]]; then
        log "ivshmem-server 已就绪"
        exit 0
    fi
    sleep 0.1
done

die "ivshmem-server 未能在预期时间内创建 socket：${SOCKET_PATH}"
