#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rd="$(run_dir)"
rec="${RECORDS_DIR}/${RUN_ID}"
serial_log="$rd/serial.log"
qemu_stderr="$rd/qemu.stderr.log"
# records 是 Day29 很重要的交付物：
# 不只是留串口日志，而是把串口 marker 切成多个可直接阅读的小文件。
ensure_dir "$rec"
cp -f "$serial_log" "$rec/serial.log"
cp -f "$qemu_stderr" "$rec/qemu.stderr.log"
cp -f "$rd/qemu-command.txt" "$rec/qemu-command.txt" 2>/dev/null || true

marker_extract 'DAY29:LSPCI_NN' "$serial_log" "$rec/lspci-nn.txt"
marker_extract 'DAY29:LSPCI_VV_NN' "$serial_log" "$rec/lspci-vv-nn.txt"
marker_extract 'DAY29:TOOL_INFO' "$serial_log" "$rec/tool-info.txt"
marker_extract 'DAY29:DEVICE_STATE_BEFORE' "$serial_log" "$rec/device-state-before.txt"
marker_extract 'DAY29:DMA_VERIFY' "$serial_log" "$rec/dma-verify.txt"
marker_extract 'DAY29:VERIFY_RESULT' "$serial_log" "$rec/verify-result.txt"
marker_extract 'DAY29:DEVICE_STATE_AFTER' "$serial_log" "$rec/device-state-after.txt"
marker_extract 'DAY29:PROC_INTERRUPTS_AFTER' "$serial_log" "$rec/proc-interrupts-after.txt"
marker_extract 'DAY29:DMESG_DRIVER' "$serial_log" "$rec/dmesg-driver.txt"

probe_logged=no
if grep -q 'probe success' "$serial_log"; then probe_logged=yes; fi
edu_visible=no
if grep -q "$EDU_DEVICE_ID_EXPECT" "$rec/lspci-nn.txt"; then edu_visible=yes; fi
guest_complete=no
if grep -q '===DAY29:COMPLETE===' "$serial_log"; then guest_complete=yes; fi
dma_alloc_ok=no
if grep -q 'dma_alloc_coherent ok' "$serial_log"; then dma_alloc_ok=yes; fi
verify_ok=no
if grep -Eq 'verify_ok=1|verify ok:' "$rec/verify-result.txt" "$rec/dma-verify.txt" "$rec/device-state-after.txt" 2>/dev/null; then
  verify_ok=yes
fi
oops_found=no
if grep -Eq 'DMA mapping error|BUG:|Oops:|Kernel panic|not syncing|hung task' "$serial_log"; then
  oops_found=yes
fi

cat > "$rec/run-summary.md" <<EOF
# Day29 Run Summary

- run id: ${RUN_ID}
- edu device visible: ${edu_visible}
- probe logged: ${probe_logged}
- dma_alloc_coherent logged: ${dma_alloc_ok}
- verify ok: ${verify_ok}
- guest flow complete: ${guest_complete}
- oops/dma-error/hung/panic found: ${oops_found}
EOF

echo "[day29] records 已生成：$rec"
