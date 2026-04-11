#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

log "开始执行 day22 全流程（RUN_ID=${RUN_ID}）"

"${SCRIPT_DIR}/00_check_host_tools.sh"
"${SCRIPT_DIR}/01_check_kernel_config.sh" || true
"${SCRIPT_DIR}/03_prepare_rootfs.sh"
"${SCRIPT_DIR}/04_prepare_ivshmem_backend.sh"
"${SCRIPT_DIR}/05_run_qemu_ivshmem.sh"

log "day22 全流程执行结束，请查看 records/${RUN_ID}/"
