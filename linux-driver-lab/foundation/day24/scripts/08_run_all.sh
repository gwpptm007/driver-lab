#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

log "开始执行 day24 全流程（RUN_ID=${RUN_ID}）"
"${SCRIPT_DIR}/00_check_host_tools.sh"
"${SCRIPT_DIR}/01_check_kernel_config.sh" || true
"${SCRIPT_DIR}/10_prepare_kernel_module_tree.sh"
"${SCRIPT_DIR}/03_build_day24_tools.sh"
"${SCRIPT_DIR}/09_build_day24_module.sh"
"${SCRIPT_DIR}/04_prepare_rootfs.sh"
"${SCRIPT_DIR}/05_prepare_ivshmem_backend.sh"
"${SCRIPT_DIR}/06_run_qemu_day24.sh"
log "QEMU 已退出，开始归档 records"
"${SCRIPT_DIR}/07_extract_records.sh"
log "day24 全流程执行结束，请查看 records/${RUN_ID}/"
