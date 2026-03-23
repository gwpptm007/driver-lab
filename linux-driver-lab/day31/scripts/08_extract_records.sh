#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 抽取 guest 串口中的 marker 段落，沉淀为 records/<RUN_ID>/ 下的独立文件。
# 这一步的目标不是“重新计算结果”，而是把 guest 侧原始证据切成后续易读、易引用的文件。
rd="$(run_dir)"
rec="${RECORDS_DIR}/${RUN_ID}"
serial_log="$rd/serial.log"
qemu_stderr="$rd/qemu.stderr.log"

ensure_dir "$rec"
cp -f "$serial_log" "$rec/serial.log"
cp -f "$qemu_stderr" "$rec/qemu.stderr.log"
cp -f "$rd/qemu-command.txt" "$rec/qemu-command.txt" 2>/dev/null || true

marker_extract 'DAY31:LSPCI_NN' "$serial_log" "$rec/lspci-nn.txt"
marker_extract 'DAY31:LSPCI_VV_NN' "$serial_log" "$rec/lspci-vv-nn.txt"
marker_extract 'DAY31:TOOL_INFO' "$serial_log" "$rec/tool-info.txt"
marker_extract 'DAY31:DEVICE_STATE_BEFORE' "$serial_log" "$rec/device-state-before.txt"
marker_extract 'DAY31:MMAP_VERIFY' "$serial_log" "$rec/mmap-verify.txt"
marker_extract 'DAY31:BENCH_IOCTL' "$serial_log" "$rec/bench-ioctl.txt"
marker_extract 'DAY31:BENCH_MMAP' "$serial_log" "$rec/bench-mmap.txt"
marker_extract 'DAY31:BENCH_DMA' "$serial_log" "$rec/bench-dma.txt"
marker_extract 'DAY31:BENCH_ALL' "$serial_log" "$rec/bench-all.txt"
marker_extract 'DAY31:RUN_RESULT' "$serial_log" "$rec/run-result.txt"
marker_extract 'DAY31:DEVICE_STATE_AFTER' "$serial_log" "$rec/device-state-after.txt"
marker_extract 'DAY31:PROC_INTERRUPTS_AFTER' "$serial_log" "$rec/proc-interrupts-after.txt"
marker_extract 'DAY31:DMESG_DRIVER' "$serial_log" "$rec/dmesg-driver.txt"

probe_logged=no
if grep -q 'probe success' "$serial_log"; then probe_logged=yes; fi
edu_visible=no
if grep -q "$EDU_DEVICE_ID_EXPECT" "$rec/lspci-nn.txt"; then edu_visible=yes; fi
guest_complete=no
if grep -q '===DAY31:COMPLETE===' "$serial_log"; then guest_complete=yes; fi
dma_alloc_ok=no
if grep -q 'dma_alloc_coherent ok' "$serial_log"; then dma_alloc_ok=yes; fi
mmap_verify_ok=no
if grep -q 'verify_ok=1' "$rec/mmap-verify.txt"; then mmap_verify_ok=yes; fi
bench_ioctl_present=no
if grep -q 'mode=ioctl' "$rec/bench-ioctl.txt" && ! grep -q 'iterations must be > 0' "$rec/bench-ioctl.txt"; then bench_ioctl_present=yes; fi
bench_mmap_present=no
if grep -q 'mode=mmap' "$rec/bench-mmap.txt" && ! grep -q 'iterations must be > 0' "$rec/bench-mmap.txt"; then bench_mmap_present=yes; fi
bench_dma_present=no
if grep -q 'mode=dma' "$rec/bench-dma.txt" && ! grep -q 'iterations must be > 0' "$rec/bench-dma.txt"; then bench_dma_present=yes; fi
bench_dma_marker_started=no
if grep -q '===DAY31:BENCH_DMA:BEGIN===' "$serial_log"; then bench_dma_marker_started=yes; fi
bench_dma_marker_ended=no
if grep -q '===DAY31:BENCH_DMA:END===' "$serial_log"; then bench_dma_marker_ended=yes; fi
# `bench_dma_partial` 的意义是：DMA 段落确实开始了，但没有形成有效统计结果。
# 这种情况常见于 guest 被 timeout 截断，或者 bench 工具异常提前返回。
bench_dma_partial=no
if [ "$bench_dma_present" = no ] && [ "$bench_dma_marker_started" = yes ]; then
  bench_dma_partial=yes
fi
# `bench_all_requested` 记录的是宿主本轮配置值，而不是 guest 最终是否真的跑出了矩阵结果。
bench_all_requested=${DAY31_RUN_BENCH_ALL:-0}
bench_all_present=no
if grep -q 'csv_header' "$rec/bench-all.txt" && ! grep -q 'iterations must be > 0' "$rec/bench-all.txt"; then bench_all_present=yes; fi
# 这里单独记录宿主 timeout 命中情况，避免把“没有 COMPLETE marker”统统误判成驱动或 guest 逻辑失败。
qemu_timeout_hit=no
if grep -q 'terminating on signal 15 from pid .*timeout' "$qemu_stderr"; then qemu_timeout_hit=yes; fi
oops_found=no
if grep -Eq 'DMA mapping error|BUG:|Oops:|Kernel panic|not syncing|hung task' "$serial_log"; then
  oops_found=yes
fi

cat > "$rec/run-summary.md" <<EOI
# Day31 Run Summary

- run id: ${RUN_ID}
- edu device visible: ${edu_visible}
- probe logged: ${probe_logged}
- dma_alloc_coherent logged: ${dma_alloc_ok}
- mmap verify ok: ${mmap_verify_ok}
- bench ioctl present: ${bench_ioctl_present}
- bench mmap present: ${bench_mmap_present}
- bench dma present: ${bench_dma_present}
- bench dma partial: ${bench_dma_partial}
- bench dma marker started: ${bench_dma_marker_started}
- bench dma marker ended: ${bench_dma_marker_ended}
- bench all requested: ${bench_all_requested}
- bench all present: ${bench_all_present}
- guest flow complete: ${guest_complete}
- qemu timeout hit: ${qemu_timeout_hit}
- oops/dma-error/hung/panic found: ${oops_found}
EOI

echo "[day31] records 已生成：$rec"
