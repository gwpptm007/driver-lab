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

# 先按 marker 切块，把串口中的结构化输出拆成单独文件。
# 这样后续分析不必在整份 serial.log 里肉眼找片段。
marker_extract 'DAY33:LSPCI_NN' "$serial_log" "$rec/lspci-nn.txt"
marker_extract 'DAY33:LSPCI_VV_NN' "$serial_log" "$rec/lspci-vv-nn.txt"
marker_extract 'DAY33:TOOL_INFO' "$serial_log" "$rec/tool-info.txt"
marker_extract 'DAY33:MMAP_VERIFY' "$serial_log" "$rec/mmap-verify.txt"
marker_extract 'DAY33:TRACE_CONFIG' "$serial_log" "$rec/trace-config.txt"
marker_extract 'DAY33:TRACE_WINDOW' "$serial_log" "$rec/trace-window.txt"
marker_extract 'DAY33:RUN_RESULT' "$serial_log" "$rec/run-result.txt"
marker_extract 'DAY33:DEVICE_STATE_AFTER' "$serial_log" "$rec/device-state-after.txt"
marker_extract 'DAY33:PROC_INTERRUPTS_AFTER' "$serial_log" "$rec/proc-interrupts-after.txt"
marker_extract 'DAY33:DMESG_DRIVER' "$serial_log" "$rec/dmesg-driver.txt"

probe_logged=no; grep -q 'probe success' "$serial_log" && probe_logged=yes || true
edu_visible=no; grep -q "$EDU_DEVICE_ID_EXPECT" "$rec/lspci-nn.txt" && edu_visible=yes || true
guest_complete=no; grep -Eq '===DAY33:COMPLETE===|===DAY33:COMPLETE_WITH_TRACE_SETUP_FAILED===' "$serial_log" && guest_complete=yes || true
dma_alloc_ok=no; grep -q 'dma_alloc_coherent ok' "$serial_log" && dma_alloc_ok=yes || true
mmap_verify_ok=no; grep -q 'verify_ok=1' "$rec/mmap-verify.txt" && mmap_verify_ok=yes || true
# Day33 这一步要明确区分三种状态：
# 1) function_graph 真正启用成功；
# 2) trace 配置失败，但 guest 优雅退出；
# 3) 旧现场那种 trace 配置失败后直接 panic。
trace_config_ok=no; grep -q 'function_graph' "$rec/trace-config.txt" && trace_config_ok=yes || true
trace_setup_failed=no; grep -q 'trace_setup_failed=' "$rec/trace-config.txt" && trace_setup_failed=yes || true
trace_window_present=no; [ -s "$rec/trace-window.txt" ] && trace_window_present=yes || true
trace_window_skipped=no; grep -q 'trace_window_skipped=trace_setup_failed' "$rec/trace-window.txt" && trace_window_skipped=yes || true
trace_ioctl_seen=no; grep -q 'day33_ioctl' "$rec/trace-window.txt" && trace_ioctl_seen=yes || true
trace_run_dma_seen=no; grep -q 'day33_do_run_dma' "$rec/trace-window.txt" && trace_run_dma_seen=yes || true
trace_wait_seen=no; grep -q 'day33_wait_dma_idle' "$rec/trace-window.txt" && trace_wait_seen=yes || true
trace_irq_seen=no; grep -q 'day33_irq_handler' "$rec/trace-window.txt" && trace_irq_seen=yes || true
qemu_timeout_hit=no; grep -q 'terminating on signal 15 from pid .*timeout' "$qemu_stderr" && qemu_timeout_hit=yes || true
oops_found=no; grep -Eq 'DMA mapping error|BUG:|Oops:|Kernel panic|not syncing|hung task' "$serial_log" && oops_found=yes || true

cat > "$rec/run-summary.md" <<EOI
# Day33 Run Summary

- run id: ${RUN_ID}
- edu device visible: ${edu_visible}
- probe logged: ${probe_logged}
- dma_alloc_coherent logged: ${dma_alloc_ok}
- mmap verify ok: ${mmap_verify_ok}
- trace config function_graph: ${trace_config_ok}
- trace setup failed marker: ${trace_setup_failed}
- trace window present: ${trace_window_present}
- trace window skipped: ${trace_window_skipped}
- trace mentions day33_ioctl: ${trace_ioctl_seen}
- trace mentions day33_do_run_dma: ${trace_run_dma_seen}
- trace mentions day33_wait_dma_idle: ${trace_wait_seen}
- trace mentions day33_irq_handler: ${trace_irq_seen}
- guest flow complete: ${guest_complete}
- qemu timeout hit: ${qemu_timeout_hit}
- oops/dma-error/hung/panic found: ${oops_found}
EOI

echo "[day33] records 已生成：$rec"
