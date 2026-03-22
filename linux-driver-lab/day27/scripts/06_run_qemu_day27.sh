#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 真正启动 QEMU。这里把 EDU 挂到 virt 机器上，guest 用 rdinit=/init 自动跑 day27 循环流程。
rd="$(run_dir)"
serial_log="$rd/serial.log"
qemu_stderr="$rd/qemu.stderr.log"
cmd=(
  "${QEMU_BIN}"
  -machine "${QEMU_MACHINE}"
  -cpu "${QEMU_CPU}"
  -m "${QEMU_MEM}"
  -nographic
  -kernel "${KERNEL_IMAGE}"
  -initrd "${ROOTFS_IMG}"
  -append "console=ttyAMA0 rdinit=/init day27_loops=${DAY27_LOOP_COUNT}"
  -device edu
)
printf '%q ' "${cmd[@]}" > "$rd/qemu-command.txt"
echo '[day27] 启动 QEMU，串口日志输出到：'"$serial_log"
"${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
if grep -q '===DAY27:COMPLETE===' "$serial_log"; then
  echo '[day27] 串口日志中已发现 day27 完成标记。'
else
  echo '[day27][WARN] 串口日志中未发现 day27 完成标记。'
  echo "[day27][WARN] 请优先查看：$serial_log 与 $qemu_stderr"
fi
