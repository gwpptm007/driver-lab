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
  -append "console=ttyAMA0 rdinit=/init day34_verify_len=${DAY34_VERIFY_LEN} day34_verify_seed=${DAY34_VERIFY_SEED} day34_concurrency_workers=${DAY34_CONCURRENCY_WORKERS} day34_concurrency_iters=${DAY34_CONCURRENCY_ITERS} day34_concurrency_len=${DAY34_CONCURRENCY_LEN} day34_ioctl_iters=${DAY34_IOCTL_ITERS} day34_module_loops=${DAY34_MODULE_LOOPS} day34_fault_len=${DAY34_FAULT_LEN} day34_fault_offset_pgoff=${DAY34_FAULT_OFFSET_PGOFF}"
  -device "edu,dma_mask=${EDU_QEMU_DMA_MASK}"
)
printf '%q ' "${cmd[@]}" > "$rd/qemu-command.txt"

echo "[day34] 启动 QEMU，串口日志输出到：$serial_log"
echo "[day34] EDU QEMU dma_mask=${EDU_QEMU_DMA_MASK} driver default dma_mask_bits=32"
echo "[day34] workers=${DAY34_CONCURRENCY_WORKERS} mmap_iters=${DAY34_CONCURRENCY_ITERS} ioctl_iters=${DAY34_IOCTL_ITERS} module_loops=${DAY34_MODULE_LOOPS}"
qemu_timeout_sec="${QEMU_TIMEOUT_SEC:-900}"
if command -v timeout >/dev/null 2>&1; then
  echo "[day34] QEMU 最长运行 ${qemu_timeout_sec}s，超时后宿主侧会自动收尾。"
  timeout --foreground "${qemu_timeout_sec}" "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
else
  "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
fi

if grep -q '===DAY34:COMPLETE===' "$serial_log"; then
  echo '[day34] 串口日志中已发现 day34 完成标记。'
else
  echo '[day34][WARN] 串口日志中未发现 day34 完成标记。'
  echo "[day34][WARN] 请优先查看：$serial_log 与 $qemu_stderr"
fi
