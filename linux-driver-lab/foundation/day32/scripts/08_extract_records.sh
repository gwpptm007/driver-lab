#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rd="$(run_dir)"
rec="${RECORDS_DIR}/${RUN_ID}"
serial_log="$rd/serial.log"
qemu_stderr="$rd/qemu.stderr.log"

ensure_dir "$rec"
cp -f "$serial_log" "$rec/serial.log"
cp -f "$qemu_stderr" "$rec/qemu.stderr.log"
cp -f "$rd/qemu-command.txt" "$rec/qemu-command.txt" 2>/dev/null || true
for extra in host-perf-baseline.stat.txt host-perf-optimized.stat.txt host-perf-baseline.report.txt host-perf-optimized.report.txt; do
  [ -f "$rd/$extra" ] && cp -f "$rd/$extra" "$rec/$extra"
done

marker_extract 'DAY32:LSPCI_NN' "$serial_log" "$rec/lspci-nn.txt"
marker_extract 'DAY32:LSPCI_VV_NN' "$serial_log" "$rec/lspci-vv-nn.txt"
marker_extract 'DAY32:TOOL_INFO' "$serial_log" "$rec/tool-info.txt"
marker_extract 'DAY32:DEVICE_STATE_BEFORE' "$serial_log" "$rec/device-state-before.txt"
marker_extract 'DAY32:MMAP_VERIFY' "$serial_log" "$rec/mmap-verify.txt"
marker_extract 'DAY32:BENCH_MMAP_BASELINE' "$serial_log" "$rec/bench-mmap-baseline.txt"
marker_extract 'DAY32:BENCH_MMAP_OPTIMIZED' "$serial_log" "$rec/bench-mmap-optimized.txt"
marker_extract 'DAY32:COMPARE_MMAP' "$serial_log" "$rec/compare-mmap.txt"
marker_extract 'DAY32:BENCH_IOCTL' "$serial_log" "$rec/bench-ioctl.txt"
marker_extract 'DAY32:BENCH_DMA_LITE' "$serial_log" "$rec/bench-dma-lite.txt"
marker_extract 'DAY32:RUN_RESULT' "$serial_log" "$rec/run-result.txt"
marker_extract 'DAY32:DEVICE_STATE_AFTER' "$serial_log" "$rec/device-state-after.txt"
marker_extract 'DAY32:PROC_INTERRUPTS_AFTER' "$serial_log" "$rec/proc-interrupts-after.txt"
marker_extract 'DAY32:DMESG_DRIVER' "$serial_log" "$rec/dmesg-driver.txt"

# 这份摘要既给人看，也给 README/验收文档复用。
# 因此判断条件尽量选“records 中稳定存在的文本信号”，避免过度依赖屏幕输出格式。
probe_logged=no; grep -q 'probe success' "$serial_log" && probe_logged=yes || true
edu_visible=no; grep -q "$EDU_DEVICE_ID_EXPECT" "$rec/lspci-nn.txt" && edu_visible=yes || true
guest_complete=no; grep -q '===DAY32:COMPLETE===' "$serial_log" && guest_complete=yes || true
dma_alloc_ok=no; grep -q 'dma_alloc_coherent ok' "$serial_log" && dma_alloc_ok=yes || true
mmap_verify_ok=no; grep -q 'verify_ok=1' "$rec/mmap-verify.txt" && mmap_verify_ok=yes || true
baseline_present=no; grep -q 'mode=mmap-baseline' "$rec/bench-mmap-baseline.txt" && baseline_present=yes || true
optimized_present=no; grep -q 'mode=mmap-optimized' "$rec/bench-mmap-optimized.txt" && optimized_present=yes || true
compare_present=no; grep -q 'compare_mode=mmap' "$rec/compare-mmap.txt" && compare_present=yes || true
ioctl_present=no; grep -q 'mode=ioctl' "$rec/bench-ioctl.txt" && ioctl_present=yes || true
dma_lite_present=no; grep -q 'mode=dma' "$rec/bench-dma-lite.txt" && dma_lite_present=yes || true
qemu_timeout_hit=no; grep -q 'terminating on signal 15 from pid .*timeout' "$qemu_stderr" && qemu_timeout_hit=yes || true
oops_found=no; grep -Eq 'DMA mapping error|BUG:|Oops:|Kernel panic|not syncing|hung task' "$serial_log" && oops_found=yes || true
host_perf_baseline=no; [ -f "$rec/host-perf-baseline.stat.txt" ] && host_perf_baseline=yes || true
host_perf_optimized=no; [ -f "$rec/host-perf-optimized.stat.txt" ] && host_perf_optimized=yes || true

get_val() {
  local file="$1" key="$2"
  awk -F= -v k="$key" '$1==k {print $2; exit}' "$file"
}
base_avg="$(get_val "$rec/bench-mmap-baseline.txt" avg_us || true)"
opt_avg="$(get_val "$rec/bench-mmap-optimized.txt" avg_us || true)"
base_p99="$(get_val "$rec/bench-mmap-baseline.txt" p99_us || true)"
opt_p99="$(get_val "$rec/bench-mmap-optimized.txt" p99_us || true)"
base_tp="$(get_val "$rec/bench-mmap-baseline.txt" throughput_mbps || true)"
opt_tp="$(get_val "$rec/bench-mmap-optimized.txt" throughput_mbps || true)"
lat_gain="$(get_val "$rec/compare-mmap.txt" avg_latency_gain_pct || true)"
p99_gain="$(get_val "$rec/compare-mmap.txt" p99_latency_gain_pct || true)"
tp_gain="$(get_val "$rec/compare-mmap.txt" throughput_gain_pct || true)"

cat > "$rec/run-summary.md" <<EOI
# Day32 Run Summary

- run id: ${RUN_ID}
- edu device visible: ${edu_visible}
- probe logged: ${probe_logged}
- dma_alloc_coherent logged: ${dma_alloc_ok}
- mmap verify ok: ${mmap_verify_ok}
- baseline bench present: ${baseline_present}
- optimized bench present: ${optimized_present}
- compare mmap present: ${compare_present}
- ioctl bench present: ${ioctl_present}
- dma lite bench present: ${dma_lite_present}
- host perf baseline present: ${host_perf_baseline}
- host perf optimized present: ${host_perf_optimized}
- guest flow complete: ${guest_complete}
- qemu timeout hit: ${qemu_timeout_hit}
- oops/dma-error/hung/panic found: ${oops_found}
- baseline avg_us: ${base_avg:-n/a}
- optimized avg_us: ${opt_avg:-n/a}
- avg latency gain pct: ${lat_gain:-n/a}
- baseline p99_us: ${base_p99:-n/a}
- optimized p99_us: ${opt_p99:-n/a}
- p99 latency gain pct: ${p99_gain:-n/a}
- baseline throughput_mbps: ${base_tp:-n/a}
- optimized throughput_mbps: ${opt_tp:-n/a}
- throughput gain pct: ${tp_gain:-n/a}
EOI

echo "[day32] records 已生成：$rec"
