#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
ensure_run_dir

ROOTFS_IMG="${WORKDIR}/rootfs.img"
SERIAL_LOG="${WORKDIR}/runs/${RUN_ID}/serial.log"
QEMU_ERR="${WORKDIR}/runs/${RUN_ID}/qemu.stderr.log"
QEMU_CMD="${WORKDIR}/runs/${RUN_ID}/qemu-command.txt"

require_file KERNEL_IMAGE "${KERNEL_IMAGE}"
require_file ROOTFS_IMG "${ROOTFS_IMG}"
require_file IVSHMEM_BACKEND_FILE "${IVSHMEM_BACKEND_FILE}"

cat > "${QEMU_CMD}" <<EOT
${QEMU_BIN} \
  -machine virt \
  -cpu cortex-a57 \
  -m 1024 \
  -nographic \
  -kernel ${KERNEL_IMAGE} \
  -initrd ${ROOTFS_IMG} \
  -append "console=ttyAMA0 rdinit=/init" \
  -object memory-backend-file,size=${IVSHMEM_SIZE},mem-path=${IVSHMEM_BACKEND_FILE},share=on,id=hostmem0 \
  -device ivshmem-plain,memdev=hostmem0
EOT

log "启动 QEMU（day24 ivshmem-plain），串口日志输出到：${SERIAL_LOG}"
rm -f "${SERIAL_LOG}" "${QEMU_ERR}"

"${QEMU_BIN}" \
  -machine virt \
  -cpu cortex-a57 \
  -m 1024 \
  -nographic \
  -kernel "${KERNEL_IMAGE}" \
  -initrd "${ROOTFS_IMG}" \
  -append "console=ttyAMA0 rdinit=/init" \
  -object memory-backend-file,size="${IVSHMEM_SIZE}",mem-path="${IVSHMEM_BACKEND_FILE}",share=on,id=hostmem0 \
  -device ivshmem-plain,memdev=hostmem0 \
  >"${SERIAL_LOG}" 2>"${QEMU_ERR}" || true

if ! grep -q '===DAY24:COMPLETE===' "${SERIAL_LOG}"; then
    warn "串口日志中未发现 day24 完成标记。"
    warn "请优先查看：${SERIAL_LOG} 与 ${QEMU_ERR}"
fi
