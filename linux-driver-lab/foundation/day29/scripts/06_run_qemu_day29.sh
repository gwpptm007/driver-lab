#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rd="$(run_dir)"
serial_log="$rd/serial.log"
qemu_stderr="$rd/qemu.stderr.log"
# 这里显式把 QEMU EDU 设备能力收口到 32-bit DMA mask，
# 对齐 Day29 驱动默认值，避免 arm64 virt 下落回默认 28-bit 后再次踩坑。
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
  -append "console=ttyAMA0 rdinit=/init day29_verify_len=${DAY29_VERIFY_LEN} day29_verify_seed=${DAY29_VERIFY_SEED}"
  -device "edu,dma_mask=${EDU_QEMU_DMA_MASK}"
)
printf '%q ' "${cmd[@]}" > "$rd/qemu-command.txt"
echo '[day29] 启动 QEMU，串口日志输出到：'"$serial_log"
echo "[day29] EDU QEMU dma_mask=${EDU_QEMU_DMA_MASK} driver default dma_mask_bits=32"
qemu_timeout_sec="${QEMU_TIMEOUT_SEC:-120}"
if command -v timeout >/dev/null 2>&1; then
  echo "[day29] QEMU 最长运行 ${qemu_timeout_sec}s，超时后宿主侧会自动收尾。"
  timeout --foreground "${qemu_timeout_sec}" "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
else
  "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
fi
if grep -q '===DAY29:COMPLETE===' "$serial_log"; then
  echo '[day29] 串口日志中已发现 day29 完成标记。'
else
  echo '[day29][WARN] 串口日志中未发现 day29 完成标记。'
  echo "[day29][WARN] 请优先查看：$serial_log 与 $qemu_stderr"
fi
