#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

require_file "${KERNEL_IMAGE}"
require_file "${ROOTFS_IMAGE}"
require_cmd "${QEMU_BIN}"

RUN_DIR="${RUNS_DIR}/${RUN_ID}"
ensure_dir "${RUN_DIR}"
SOCKET_PATH="${RUN_DIR}/ivshmem.sock"
PIDFILE="${RUN_DIR}/ivshmem-server.pid"
SERIAL_LOG="${RUN_DIR}/serial.log"
QEMU_STDERR="${RUN_DIR}/qemu.stderr.log"
QEMU_STDOUT="${RUN_DIR}/qemu.stdout.log"
QEMU_CMD_TXT="${RUN_DIR}/qemu-command.txt"

# 没有 server 就先启动；这样单独跑 qemu 脚本也能工作。
if [[ ! -S "${SOCKET_PATH}" ]]; then
    log "未发现 ivshmem server socket，先自动启动 server。"
    "${SCRIPT_DIR}/04_start_ivshmem_server.sh"
fi

# QEMU 启动参数全部显式写出来，方便 day22 之后做证据归档和排错。
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
    -chardev "socket,path=${SOCKET_PATH},id=ivshmem0"
    -device "ivshmem-doorbell,vectors=${IVSHMEM_VECTORS},chardev=ivshmem0"
)

printf '%q ' "${QEMU_CMD[@]}" > "${QEMU_CMD_TXT}"
printf '\n' >> "${QEMU_CMD_TXT}"

log "启动 QEMU，串口日志输出到：${SERIAL_LOG}"

set +e
"${TIMEOUT_BIN}" "${QEMU_TIMEOUT_SEC}" "${QEMU_CMD[@]}" >"${QEMU_STDOUT}" 2>"${QEMU_STDERR}"
qemu_rc=$?
set -e

# timeout 退出码 124 表示超时；对 day22 来说属于真实失败，要直接报错。
if [[ ${qemu_rc} -eq 124 ]]; then
    die "QEMU 运行超时（${QEMU_TIMEOUT_SEC}s）。请检查串口日志：${SERIAL_LOG}"
fi

# QEMU 被 guest poweroff 触发退出时，常见返回值可能为 0 或 1。
# 这里不机械地只接受 0，而是用串口日志内的完成标记做最终判断。
if ! ${GREP_BIN} -q '^===DAY22:COMPLETE===$' "${SERIAL_LOG}"; then
    warn "串口日志中未发现 day22 完成标记。"
    warn "请优先查看：${SERIAL_LOG} 与 ${QEMU_STDERR}"
fi

log "QEMU 已退出，开始归档 records"
"${SCRIPT_DIR}/06_extract_records.sh"
