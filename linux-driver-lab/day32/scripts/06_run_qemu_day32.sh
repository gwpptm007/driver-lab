#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rd="$(run_dir)"
serial_log="$rd/serial.log"
qemu_stderr="$rd/qemu.stderr.log"

# 统一把 day32 workload 参数通过 kernel cmdline 送进 guest，
# 这样 run/perf-baseline/perf-optimized 三条宿主入口只需改环境变量，不必改 guest 镜像。
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
  -append "console=ttyAMA0 rdinit=/init day32_verify_len=${DAY32_VERIFY_LEN} day32_verify_seed=${DAY32_VERIFY_SEED} day32_perf_len=${DAY32_PERF_LEN} day32_perf_seed=${DAY32_PERF_SEED} day32_perf_iter=${DAY32_PERF_ITER} day32_perf_warmup=${DAY32_PERF_WARMUP} day32_dma_lite_len=${DAY32_DMA_LITE_LEN} day32_dma_lite_iter=${DAY32_DMA_LITE_ITER} day32_dma_lite_warmup=${DAY32_DMA_LITE_WARMUP} day32_profile_mode=${DAY32_PROFILE_MODE} day32_run_dma_lite=${DAY32_RUN_DMA_LITE}"
  -device "edu,dma_mask=${EDU_QEMU_DMA_MASK}"
)
printf '%q ' "${cmd[@]}" > "$rd/qemu-command.txt"

echo "[day32] 启动 QEMU，串口日志输出到：$serial_log"
echo "[day32] EDU QEMU dma_mask=${EDU_QEMU_DMA_MASK} driver default dma_mask_bits=32"
echo "[day32] perf_len=${DAY32_PERF_LEN} iter/warmup=${DAY32_PERF_ITER}/${DAY32_PERF_WARMUP} profile_mode=${DAY32_PROFILE_MODE} run_dma_lite=${DAY32_RUN_DMA_LITE}"

# perf 与 compare-mmap 都属于完整 workload，默认 timeout 需要明显高于普通验证用例。
qemu_timeout_sec="${QEMU_TIMEOUT_SEC:-240}"
if command -v timeout >/dev/null 2>&1; then
  echo "[day32] QEMU 最长运行 ${qemu_timeout_sec}s，超时后宿主侧会自动收尾。"
  timeout --foreground "${qemu_timeout_sec}" "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
else
  "${cmd[@]}" > "$serial_log" 2> "$qemu_stderr" || true
fi

if grep -q '===DAY32:COMPLETE===' "$serial_log"; then
  echo '[day32] 串口日志中已发现 day32 完成标记。'
else
  echo '[day32][WARN] 串口日志中未发现 day32 完成标记。'
  echo "[day32][WARN] 请优先查看：$serial_log 与 $qemu_stderr"
fi
