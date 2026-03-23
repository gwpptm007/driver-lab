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

marker_extract 'DAY30:LSPCI_NN' "$serial_log" "$rec/lspci-nn.txt"
marker_extract 'DAY30:LSPCI_VV_NN' "$serial_log" "$rec/lspci-vv-nn.txt"
marker_extract 'DAY30:TOOL_INFO' "$serial_log" "$rec/tool-info.txt"
marker_extract 'DAY30:DEVICE_STATE_BEFORE' "$serial_log" "$rec/device-state-before.txt"
marker_extract 'DAY30:INVALID_MMAP_LEN' "$serial_log" "$rec/invalid-mmap-len.txt"
marker_extract 'DAY30:INVALID_MMAP_OFFSET' "$serial_log" "$rec/invalid-mmap-offset.txt"
marker_extract 'DAY30:MMAP_VERIFY' "$serial_log" "$rec/mmap-verify.txt"
marker_extract 'DAY30:RUN_RESULT' "$serial_log" "$rec/run-result.txt"
marker_extract 'DAY30:DEVICE_STATE_AFTER' "$serial_log" "$rec/device-state-after.txt"
marker_extract 'DAY30:PROC_INTERRUPTS_AFTER' "$serial_log" "$rec/proc-interrupts-after.txt"
marker_extract 'DAY30:DMESG_DRIVER' "$serial_log" "$rec/dmesg-driver.txt"

probe_logged=no
if grep -q 'probe success' "$serial_log"; then probe_logged=yes; fi
edu_visible=no
if grep -q "$EDU_DEVICE_ID_EXPECT" "$rec/lspci-nn.txt"; then edu_visible=yes; fi
guest_complete=no
if grep -q '===DAY30:COMPLETE===' "$serial_log"; then guest_complete=yes; fi
dma_alloc_ok=no
if grep -q 'dma_alloc_coherent ok' "$serial_log"; then dma_alloc_ok=yes; fi
mmap_verify_ok=no
if grep -q 'verify_ok=1' "$rec/mmap-verify.txt"; then mmap_verify_ok=yes; fi
run_ok=no
if grep -q 'run_ok=1' "$rec/run-result.txt"; then run_ok=yes; fi
# 注意：invalid-mmap-len 是否通过，既取决于驱动 mmap 边界，也取决于 guest
# 用例本身是否真的构造了“VMA 长度 != map_bytes”。早期使用 2048 时会在 4KB 页
# 环境里被扩成 4096，导致样例本身不再是非法请求。
invalid_len_rejected=no
if grep -q 'expected failure: invalid length rejected' "$rec/invalid-mmap-len.txt"; then invalid_len_rejected=yes; fi
invalid_offset_rejected=no
if grep -q 'expected failure: invalid offset rejected' "$rec/invalid-mmap-offset.txt"; then invalid_offset_rejected=yes; fi
oops_found=no
if grep -Eq 'DMA mapping error|BUG:|Oops:|Kernel panic|not syncing|hung task' "$serial_log"; then
  oops_found=yes
fi

cat > "$rec/run-summary.md" <<EOI
# Day30 Run Summary

- run id: ${RUN_ID}
- edu device visible: ${edu_visible}
- probe logged: ${probe_logged}
- dma_alloc_coherent logged: ${dma_alloc_ok}
- user mmap verify ok: ${mmap_verify_ok}
- driver run ok: ${run_ok}
- invalid mmap len rejected: ${invalid_len_rejected}
- invalid mmap offset rejected: ${invalid_offset_rejected}
- guest flow complete: ${guest_complete}
- oops/dma-error/hung/panic found: ${oops_found}
EOI

echo "[day30] records 已生成：$rec"
