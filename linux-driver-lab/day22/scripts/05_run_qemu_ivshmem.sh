#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

require_file "${KERNEL_IMAGE}"
require_file "${ROOTFS_IMAGE}"
require_cmd "${QEMU_BIN}"

RUN_DIR="${RUNS_DIR}/${RUN_ID}"
ensure_dir "${RUN_DIR}"
BACKEND_FILE="${RUN_DIR}/${IVSHMEM_BACKEND_FILENAME}"
SERIAL_LOG="${RUN_DIR}/serial.log"
QEMU_STDERR="${RUN_DIR}/qemu.stderr.log"
QEMU_STDOUT="${RUN_DIR}/qemu.stdout.log"
QEMU_CMD_TXT="${RUN_DIR}/qemu-command.txt"

if [[ ! -f "${BACKEND_FILE}" ]]; then
    log "未发现 ivshmem backend 文件，先自动准备。"
    "${SCRIPT_DIR}/04_prepare_ivshmem_backend.sh"
fi

QEMU_CMD=(
    "${QEMU_BIN}"
    -machine "${QEMU_MACHINE}"
    -cpu "${QEMU_CPU}"
    -m "${QEMU_MEMORY_MB}"
    -accel "${QEMU_ACCEL}"
    -kernel "${KERNEL_IMAGE}"
    -initrd "${ROOTFS_IMAGE}"
    -append "${QEMU_APPEND}"
    -display none
    -no-reboot
    -monitor none
    -serial "file:${SERIAL_LOG}"
    -object "memory-backend-file,size=${IVSHMEM_SIZE},share=on,mem-path=${BACKEND_FILE},id=ivshmem0"
    -device "ivshmem-plain,memdev=ivshmem0"
)

printf '%q ' "${QEMU_CMD[@]}" > "${QEMU_CMD_TXT}"
printf '
' >> "${QEMU_CMD_TXT}"

log "启动 QEMU（ivshmem-plain），串口日志输出到：${SERIAL_LOG}"

set +e
"${TIMEOUT_BIN}" "${QEMU_TIMEOUT_SEC}" "${QEMU_CMD[@]}" >"${QEMU_STDOUT}" 2>"${QEMU_STDERR}"
qemu_rc=$?
set -e

if [[ ${qemu_rc} -eq 124 ]]; then
    die "QEMU 运行超时（${QEMU_TIMEOUT_SEC}s）。请检查串口日志：${SERIAL_LOG}"
fi

if ! ${GREP_BIN} -q '^===DAY22:COMPLETE===$' "${SERIAL_LOG}"; then
    warn "串口日志中未发现 day22 完成标记。"
    warn "请优先查看：${SERIAL_LOG} 与 ${QEMU_STDERR}"
fi

log "QEMU 已退出，开始归档 records"
"${SCRIPT_DIR}/06_extract_records.sh"
