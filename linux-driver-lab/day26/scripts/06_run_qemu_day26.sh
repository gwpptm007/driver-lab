#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rd="$(run_dir)"
serial_log="$rd/serial.log"
qemu_err="$rd/qemu.stderr.log"
qemu_cmd="$rd/qemu-command.txt"

# 把最终启动命令原样落盘，方便后续排障与复现。
cat > "$qemu_cmd" <<EOCMD
${QEMU_BIN} \
  -machine ${QEMU_MACHINE} \
  -cpu ${QEMU_CPU} \
  -m ${QEMU_MEM} \
  -nographic \
  -kernel ${KERNEL_IMAGE} \
  -initrd ${ROOTFS_IMG} \
  -append "console=ttyAMA0 rdinit=/init" \
  -device edu
EOCMD

echo "[day26] 启动 QEMU（EDU + tool + clear errno），串口日志输出到：${serial_log}"
"${QEMU_BIN}" \
  -machine "${QEMU_MACHINE}" \
  -cpu "${QEMU_CPU}" \
  -m "${QEMU_MEM}" \
  -nographic \
  -kernel "${KERNEL_IMAGE}" \
  -initrd "${ROOTFS_IMG}" \
  -append "console=ttyAMA0 rdinit=/init" \
  -device edu \
  >"${serial_log}" 2>"${qemu_err}" || true

if ! grep -q '===DAY26:COMPLETE===' "$serial_log"; then
    echo "[day26][WARN] 串口日志中未发现 day26 完成标记。"
    echo "[day26][WARN] 请优先查看：${serial_log} 与 ${qemu_err}"
fi
