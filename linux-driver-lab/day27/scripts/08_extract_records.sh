#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 将本次 run 的临时产物复制到 records/<RUN_ID>/ 下，便于最终交付时直接归档。
rd="$(run_dir)"
rec="${RECORDS_DIR}/${RUN_ID}"
serial_log="$rd/serial.log"
qemu_stderr="$rd/qemu.stderr.log"
ensure_dir "$rec"
cp -f "$serial_log" "$rec/serial.log"
cp -f "$qemu_stderr" "$rec/qemu.stderr.log"
cp -f "$rd/qemu-command.txt" "$rec/qemu-command.txt" 2>/dev/null || true

# 通过 marker block 从完整串口日志中切出关键证据文件。
marker_extract 'DAY27:LSPCI_NN' "$serial_log" "$rec/lspci-nn.txt"
marker_extract 'DAY27:LSPCI_VV_NN' "$serial_log" "$rec/lspci-vv-nn.txt"
marker_extract 'DAY27:LOOP_SUMMARY' "$serial_log" "$rec/loop-summary.txt"
marker_extract 'DAY27:PROC_INTERRUPTS_FINAL' "$serial_log" "$rec/proc-interrupts-final.txt"
marker_extract 'DAY27:DMESG_DRIVER' "$serial_log" "$rec/dmesg-driver.txt"

# 统计关键日志次数。Day27 的“是否通过”主要就看：
# 1. probe/remove 是否反复成功；
# 2. irq handler 是否真的被触发；
# 3. 200 次循环是否全部 pass。
probe_count=$(grep -c 'probe success' "$serial_log" || true)
remove_count=$(grep -c 'remove leave' "$serial_log" || true)
irq_count=$(grep -c 'irq handler:' "$serial_log" || true)

# loop-summary.txt 是从串口切出来的，行尾可能带 ，因此这里统一去掉回车和空白。
loop_pass=$(awk -F= '/^pass=/{print $2}' "$rec/loop-summary.txt" 2>/dev/null | tail -n1 | tr -d '[:space:]')
loop_fail=$(awk -F= '/^fail=/{print $2}' "$rec/loop-summary.txt" 2>/dev/null | tail -n1 | tr -d '[:space:]')
[ -n "$loop_pass" ] || loop_pass=0
[ -n "$loop_fail" ] || loop_fail=0

guest_complete=no
if grep -q '===DAY27:COMPLETE===' "$serial_log"; then guest_complete=yes; fi
edu_visible=no
if grep -q "$EDU_DEVICE_ID_EXPECT" "$rec/lspci-nn.txt"; then edu_visible=yes; fi
probe_logged=no
if [ "$probe_count" -gt 0 ]; then probe_logged=yes; fi
remove_logged=no
if [ "$remove_count" -gt 0 ]; then remove_logged=yes; fi
irq_logged=no
if [ "$irq_count" -gt 0 ]; then irq_logged=yes; fi
loop_target_met=no
if [ "$loop_pass" -eq "$DAY27_LOOP_COUNT" ] && [ "$loop_fail" -eq 0 ]; then loop_target_met=yes; fi
oops_found=no
if grep -Eq 'BUG:|Oops:|Kernel panic|not syncing|hung task' "$serial_log"; then oops_found=yes; fi

cat > "$rec/run-summary.md" <<EOF
# Day27 Run Summary

- run id: ${RUN_ID}
- edu device visible: ${edu_visible}
- probe logged: ${probe_logged}
- remove logged: ${remove_logged}
- irq handler logged: ${irq_logged}
- loop target met (${DAY27_LOOP_COUNT}): ${loop_target_met}
- guest flow complete: ${guest_complete}
- oops/hung/panic found: ${oops_found}

- probe success count: ${probe_count}
- remove leave count: ${remove_count}
- irq handler count: ${irq_count}
- loop pass: ${loop_pass}
- loop fail: ${loop_fail}
EOF

echo "[day27] records 已生成：$rec"
