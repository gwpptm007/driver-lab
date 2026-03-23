#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 运行目录把“本轮 QEMU 命令、串口日志、stderr”放在一起，
# 便于后续 records 抽取脚本统一归档。
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
  -append "console=ttyAMA0 rdinit=/init day31_verify_len=${DAY31_VERIFY_LEN} day31_verify_seed=${DAY31_VERIFY_SEED} day31_bench_iter=${DAY31_BENCH_ITER} day31_bench_warmup=${DAY31_BENCH_WARMUP} day31_run_bench_all=${DAY31_RUN_BENCH_ALL}"
  -device "edu,dma_mask=${EDU_QEMU_DMA_MASK}"
)
printf '%q ' "${cmd[@]}" > "$rd/qemu-command.txt"

echo "[day31] 启动 QEMU，串口日志输出到：$serial_log"
echo "[day31] EDU QEMU dma_mask=${EDU_QEMU_DMA_MASK} driver default dma_mask_bits=32"
echo "[day31] bench iter/warmup=${DAY31_BENCH_ITER}/${DAY31_BENCH_WARMUP} run_bench_all=${DAY31_RUN_BENCH_ALL}"

# QEMU timeout 只负责宿主侧兜底，防止 guest 异常时一直挂住。
# 但 day31 的 DMA bench 本身较慢：如果迭代数较大，timeout 过小会把“正常执行中的 DMA bench”误判成超时。
qemu_timeout_sec="${QEMU_TIMEOUT_SEC:-360}"
if command -v timeout >/dev/null 2>&1; then
  echo "[day31] QEMU 最长运行 ${qemu_timeout_sec}s，超时后宿主侧会自动收尾。"
  timeout --foreground "${qemu_timeout_sec}" "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
else
  "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
fi

if grep -q '===DAY31:COMPLETE===' "$serial_log"; then
  echo '[day31] 串口日志中已发现 day31 完成标记。'
else
  echo '[day31][WARN] 串口日志中未发现 day31 完成标记。'
  echo "[day31][WARN] 请优先查看：$serial_log 与 $qemu_stderr"
fi
