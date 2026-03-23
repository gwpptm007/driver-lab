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
  -append "console=ttyAMA0 rdinit=/init day33_verify_len=${DAY33_VERIFY_LEN} day33_verify_seed=${DAY33_VERIFY_SEED} day33_trace_len=${DAY33_TRACE_LEN} day33_trace_seed=${DAY33_TRACE_SEED} day33_trace_workload=${DAY33_TRACE_WORKLOAD} day33_trace_dma_iter=${DAY33_TRACE_DMA_ITER} day33_trace_dma_warmup=${DAY33_TRACE_DMA_WARMUP}"
  -device "edu,dma_mask=${EDU_QEMU_DMA_MASK}"
)
printf '%q ' "${cmd[@]}" > "$rd/qemu-command.txt"

echo "[day33] 启动 QEMU，串口日志输出到：$serial_log"
echo "[day33] EDU QEMU dma_mask=${EDU_QEMU_DMA_MASK} driver default dma_mask_bits=32"
echo "[day33] trace workload=${DAY33_TRACE_WORKLOAD} len=${DAY33_TRACE_LEN} seed=${DAY33_TRACE_SEED} dma_iter=${DAY33_TRACE_DMA_ITER}"

qemu_timeout_sec="${QEMU_TIMEOUT_SEC:-180}"
if command -v timeout >/dev/null 2>&1; then
  echo "[day33] QEMU 最长运行 ${qemu_timeout_sec}s，超时后宿主侧会自动收尾。"
  timeout --foreground "${qemu_timeout_sec}" "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
else
  "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
fi

if grep -q '===DAY33:COMPLETE===' "$serial_log"; then
  echo '[day33] 串口日志中已发现 day33 完成标记。'
else
  echo '[day33][WARN] 串口日志中未发现 day33 完成标记。'
  echo "[day33][WARN] 请优先查看：$serial_log 与 $qemu_stderr"
fi
