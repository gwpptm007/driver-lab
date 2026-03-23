#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rd="$(run_dir)"
serial_log="$rd/serial.log"
qemu_stderr="$rd/qemu.stderr.log"

cmd=(
  "${QEMU_BIN}"
  -machine "${QEMU_MACHINE}"
  -cpu "${QEMU_CPU}"
  -m "${QEMU_MEM}"
  -nographic
  -no-reboot
  -monitor none
  -kernel "${KERNEL_IMAGE}"
  -initrd "${ROOTFS_IMG}"
  -append "console=ttyAMA0 rdinit=/init day30_verify_len=${DAY30_VERIFY_LEN} day30_verify_seed=${DAY30_VERIFY_SEED}"
  -device "edu,dma_mask=${EDU_QEMU_DMA_MASK}"
)
printf '%q ' "${cmd[@]}" > "$rd/qemu-command.txt"

echo "[day30] 启动 QEMU，串口日志输出到：$serial_log"
echo "[day30] EDU QEMU dma_mask=${EDU_QEMU_DMA_MASK} driver default dma_mask_bits=32"

# QEMU 侧加超时兜底，避免 guest 因脚本错误、panic 或未 poweroff 时，宿主侧
# 一直阻塞在 qemu 进程上。
qemu_timeout_sec="${QEMU_TIMEOUT_SEC:-120}"
if command -v timeout >/dev/null 2>&1; then
  echo "[day30] QEMU 最长运行 ${qemu_timeout_sec}s，超时后宿主侧会自动收尾。"
  timeout --foreground "${qemu_timeout_sec}" "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
else
  "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
fi

if grep -q '===DAY30:COMPLETE===' "$serial_log"; then
  echo '[day30] 串口日志中已发现 day30 完成标记。'
else
  echo '[day30][WARN] 串口日志中未发现 day30 完成标记。'
  echo "[day30][WARN] 请优先查看：$serial_log 与 $qemu_stderr"
fi
