#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 这一步只负责“把 day25 的内核 + initramfs + QEMU EDU 设备拉起来”，
# 不负责任何 records 解析。records 统一留给 08_extract_records.sh 处理。
rd="$(run_dir)"
serial_log="$rd/serial.log"
qemu_err="$rd/qemu.stderr.log"
qemu_cmd="$rd/qemu-command.txt"

cat > "$qemu_cmd" <<EOCMD
${QEMU_BIN}   -machine ${QEMU_MACHINE}   -cpu ${QEMU_CPU}   -m ${QEMU_MEM}   -nographic   -kernel ${KERNEL_IMAGE}   -initrd ${ROOTFS_IMG}   -append "console=ttyAMA0 rdinit=/init"   -device edu
EOCMD

echo "[day25] 启动 QEMU（EDU + MSI），串口日志输出到：${serial_log}"
"${QEMU_BIN}"   -machine "${QEMU_MACHINE}"   -cpu "${QEMU_CPU}"   -m "${QEMU_MEM}"   -nographic   -kernel "${KERNEL_IMAGE}"   -initrd "${ROOTFS_IMG}"   -append "console=ttyAMA0 rdinit=/init"   -device edu   >"${serial_log}" 2>"${qemu_err}" || true

if ! grep -q '===DAY25:COMPLETE===' "$serial_log"; then
    echo "[day25][WARN] 串口日志中未发现 day25 完成标记。"
    echo "[day25][WARN] 请优先查看：${serial_log} 与 ${qemu_err}"
fi
