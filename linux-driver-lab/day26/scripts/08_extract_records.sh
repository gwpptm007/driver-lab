#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rd="$(run_dir)"
out="${RECORDS_DIR}/${RUN_ID}"
serial="${rd}/serial.log"
qerr="${rd}/qemu.stderr.log"
qcmd="${rd}/qemu-command.txt"

ensure_dir "${out}"
cp -f "$serial" "${out}/serial.log"
cp -f "$qerr" "${out}/qemu.stderr.log"
cp -f "$qcmd" "${out}/qemu-command.txt"

# 从 serial.log 中切 marker block。这样 records/ 里的每个文件都能直接对应 guest/init.day26
# 里的某一段输出，便于验收和定位。
extract_block() {
    local begin="$1" end="$2" dest="$3"
    awk -v b="$begin" -v e="$end" '
        $0 ~ b {f=1; next}
        $0 ~ e {f=0; exit}
        f {print}
    ' "$serial" > "$dest" || true
}

extract_block '===DAY26:LSPCI_NN:BEGIN===' '===DAY26:LSPCI_NN:END===' "${out}/lspci-nn.txt"
extract_block '===DAY26:LSPCI_VV_NN:BEGIN===' '===DAY26:LSPCI_VV_NN:END===' "${out}/lspci-vv-nn.txt"
extract_block '===DAY26:INFO_BEFORE:BEGIN===' '===DAY26:INFO_BEFORE:END===' "${out}/info-before.txt"
extract_block '===DAY26:READ_STATE_BEFORE:BEGIN===' '===DAY26:READ_STATE_BEFORE:END===' "${out}/read-state-before.txt"
extract_block '===DAY26:IRQ_COUNT_BEFORE:BEGIN===' '===DAY26:IRQ_COUNT_BEFORE:END===' "${out}/irq-count-before.txt"
extract_block '===DAY26:IRQ_STATUS_BEFORE:BEGIN===' '===DAY26:IRQ_STATUS_BEFORE:END===' "${out}/irq-status-before.txt"
extract_block '===DAY26:PROC_INTERRUPTS_BEFORE:BEGIN===' '===DAY26:PROC_INTERRUPTS_BEFORE:END===' "${out}/proc-interrupts-before.txt"
extract_block '===DAY26:TRIGGER:BEGIN===' '===DAY26:TRIGGER:END===' "${out}/trigger.txt"
extract_block '===DAY26:IRQ_COUNT_AFTER:BEGIN===' '===DAY26:IRQ_COUNT_AFTER:END===' "${out}/irq-count-after.txt"
extract_block '===DAY26:IRQ_STATUS_AFTER:BEGIN===' '===DAY26:IRQ_STATUS_AFTER:END===' "${out}/irq-status-after.txt"
extract_block '===DAY26:READ_STATE_AFTER:BEGIN===' '===DAY26:READ_STATE_AFTER:END===' "${out}/read-state-after.txt"
extract_block '===DAY26:PROC_INTERRUPTS_AFTER:BEGIN===' '===DAY26:PROC_INTERRUPTS_AFTER:END===' "${out}/proc-interrupts-after.txt"
extract_block '===DAY26:INVALID_TRIGGER_ZERO:BEGIN===' '===DAY26:INVALID_TRIGGER_ZERO:END===' "${out}/invalid-trigger-zero.txt"
extract_block '===DAY26:RESET_STATS:BEGIN===' '===DAY26:RESET_STATS:END===' "${out}/reset-stats.txt"
extract_block '===DAY26:DMESG_DRIVER:BEGIN===' '===DAY26:DMESG_DRIVER:END===' "${out}/dmesg-driver.txt"

# 基于提取出的证据文件生成摘要。
{
    echo "# day26 run summary"
    echo
    if grep -q '1234:11e8' "${out}/lspci-nn.txt"; then echo "- edu device visible: yes"; else echo "- edu device visible: no"; fi
    if grep -q 'probe success' "${out}/dmesg-driver.txt"; then echo "- probe success: yes"; else echo "- probe success: no"; fi
    if grep -q 'tool_api_version=' "${out}/info-before.txt"; then echo "- ioctl info works: yes"; else echo "- ioctl info works: no"; fi
    if grep -q 'vendor=0x1234 device=0x11e8' "${out}/read-state-before.txt"; then echo "- read state works: yes"; else echo "- read state works: no"; fi
    before=$(grep -o '[0-9][0-9]*' "${out}/irq-count-before.txt" | tail -n1 || echo 0)
    after=$(grep -o '[0-9][0-9]*' "${out}/irq-count-after.txt" | tail -n1 || echo 0)
    if [ ${after:-0} -gt ${before:-0} ]; then echo "- driver irq_count grows: yes"; else echo "- driver irq_count grows: no"; fi
    p_before=$(awk '/day26_edu_tool/ {print $2; exit}' "${out}/proc-interrupts-before.txt" 2>/dev/null || echo 0)
    p_after=$(awk '/day26_edu_tool/ {print $2; exit}' "${out}/proc-interrupts-after.txt" 2>/dev/null || echo 0)
    if grep -q 'day26_edu_tool' "${out}/proc-interrupts-after.txt"; then echo "- /proc/interrupts entry exists: yes"; else echo "- /proc/interrupts entry exists: no"; fi
    if [ ${p_after:-0} -gt ${p_before:-0} ]; then echo "- /proc/interrupts count grows: yes"; else echo "- /proc/interrupts count grows: no"; fi
    if grep -q 'Invalid argument' "${out}/invalid-trigger-zero.txt" && grep -q 'rc=5' "${out}/invalid-trigger-zero.txt"; then echo "- invalid trigger error clear: yes"; else echo "- invalid trigger error clear: no"; fi
    if grep -q '===DAY26:COMPLETE===' "$serial"; then echo "- guest flow complete: yes"; else echo "- guest flow complete: no"; fi
} > "${out}/run-summary.md"

echo "[day26] records 已生成：${out}"
