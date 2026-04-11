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

marker_extract 'DAY34:LSPCI_NN' "$serial_log" "$rec/lspci-nn.txt"
marker_extract 'DAY34:LSPCI_VV_NN' "$serial_log" "$rec/lspci-vv-nn.txt"
marker_extract 'DAY34:TOOL_INFO' "$serial_log" "$rec/tool-info.txt"
marker_extract 'DAY34:MMAP_VERIFY' "$serial_log" "$rec/mmap-verify.txt"
marker_extract 'DAY34:CONCURRENT_STRESS' "$serial_log" "$rec/concurrent-stress.txt"
marker_extract 'DAY34:MODULE_LOOP' "$serial_log" "$rec/module-loop.txt"
marker_extract 'DAY34:FAULT_INVALID_LEN' "$serial_log" "$rec/fault-invalid-len.txt"
marker_extract 'DAY34:FAULT_MMAP_OFFSET' "$serial_log" "$rec/fault-mmap-offset.txt"
# run-result 只表示“最后一次操作”的驱动状态快照；在当前 day34 自动化里，
# 它通常发生在两条 fault 注入之后，因此更适合拿来解释最近一次错误路径是否
# 被正确记录，而不是充当整轮稳定性回归的总汇总。
marker_extract 'DAY34:RUN_RESULT' "$serial_log" "$rec/run-result.txt"
marker_extract 'DAY34:DEVICE_STATE_AFTER' "$serial_log" "$rec/device-state-after.txt"
marker_extract 'DAY34:PROC_INTERRUPTS_AFTER' "$serial_log" "$rec/proc-interrupts-after.txt"
marker_extract 'DAY34:DMESG_DRIVER' "$serial_log" "$rec/dmesg-driver.txt"

probe_logged=no; grep -q 'probe success' "$serial_log" && probe_logged=yes || true
lspci_available=yes
if grep -q 'lspci: not found' "$rec/lspci-nn.txt" 2>/dev/null; then
  lspci_available=no
fi
edu_visible=no
if [ "$lspci_available" = yes ] && grep -q "$EDU_DEVICE_ID_EXPECT" "$rec/lspci-nn.txt"; then
  edu_visible=yes
fi
dma_alloc_ok=no; grep -q 'dma_alloc_coherent ok' "$serial_log" && dma_alloc_ok=yes || true
mmap_verify_ok=no; grep -q 'verify_ok=1' "$rec/mmap-verify.txt" && mmap_verify_ok=yes || true
concurrent_present=no; [ -s "$rec/concurrent-stress.txt" ] && concurrent_present=yes || true
concurrent_ok=no; grep -q 'worker_fail=0' "$rec/concurrent-stress.txt" && concurrent_ok=yes || true
module_loop_present=no; [ -s "$rec/module-loop.txt" ] && module_loop_present=yes || true
module_loop_ok=no
req_loops=$(grep -m1 '^requested_loops=' "$rec/module-loop.txt" 2>/dev/null | cut -d= -f2 || true)
completed_loops=$(grep -m1 '^completed_loops=' "$rec/module-loop.txt" 2>/dev/null | cut -d= -f2 || true)
failed_loops=$(grep -m1 '^failed_loops=' "$rec/module-loop.txt" 2>/dev/null | cut -d= -f2 || true)
if [ -n "$req_loops" ] && [ "$req_loops" = "$completed_loops" ] && [ "${failed_loops:-1}" = "0" ]; then
  module_loop_ok=yes
fi
fault_invalid_len_ok=no; grep -q 'expected_failure=1' "$rec/fault-invalid-len.txt" && fault_invalid_len_ok=yes || true
fault_mmap_offset_ok=no; grep -q 'expected_failure=1' "$rec/fault-mmap-offset.txt" && fault_mmap_offset_ok=yes || true
guest_complete=no; grep -q '===DAY34:COMPLETE===' "$serial_log" && guest_complete=yes || true
qemu_timeout_hit=no; grep -q 'terminating on signal 15 from pid .*timeout' "$qemu_stderr" && qemu_timeout_hit=yes || true
oops_found=no; grep -Eq 'DMA mapping error|BUG:|Oops:|Kernel panic|not syncing|hung task' "$serial_log" && oops_found=yes || true

# run-summary.md 才是 Day34 默认验收的总入口：它把
# - mmap-verify
# - concurrent stress
# - module loop
# - fault injection
# - guest complete / timeout / oops
# 这些证据合并起来，避免被 run-result 的“最后一次操作快照”误导。
cat > "$rec/run-summary.md" <<EOI
# Day34 Run Summary

- run id: ${RUN_ID}
- lspci available in guest: ${lspci_available}
- edu device visible: ${edu_visible}
- probe logged: ${probe_logged}
- dma_alloc_coherent logged: ${dma_alloc_ok}
- mmap verify ok: ${mmap_verify_ok}
- concurrent stress present: ${concurrent_present}
- concurrent stress ok: ${concurrent_ok}
- module loop present: ${module_loop_present}
- module loop ok: ${module_loop_ok}
- fault invalid len ok: ${fault_invalid_len_ok}
- fault mmap offset ok: ${fault_mmap_offset_ok}
- guest flow complete: ${guest_complete}
- qemu timeout hit: ${qemu_timeout_hit}
- oops/dma-error/hung/panic found: ${oops_found}
EOI

echo "[day34] records 已生成：$rec"
