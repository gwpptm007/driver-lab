#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 从串口大日志中切分出各个 marker 对应的小文件，
# 这样 README / 验收文档可以直接针对单个证据文件解释。
rd="$(run_dir)"
out="${RECORDS_DIR}/${RUN_ID}"
serial="${rd}/serial.log"
qerr="${rd}/qemu.stderr.log"
qcmd="${rd}/qemu-command.txt"

ensure_dir "${out}"
cp -f "$serial" "${out}/serial.log"
cp -f "$qerr" "${out}/qemu.stderr.log"
cp -f "$qcmd" "${out}/qemu-command.txt"

extract_block() {
    local begin="$1" end="$2" dest="$3"
    awk -v b="$begin" -v e="$end" '
        $0 ~ b {f=1; next}
        $0 ~ e {f=0; exit}
        f {print}
    ' "$serial" > "$dest" || true
}

extract_block '===DAY25:LSPCI_NN:BEGIN===' '===DAY25:LSPCI_NN:END===' "${out}/lspci-nn.txt"
extract_block '===DAY25:LSPCI_VV_NN:BEGIN===' '===DAY25:LSPCI_VV_NN:END===' "${out}/lspci-vv-nn.txt"
extract_block '===DAY25:IRQ_INFO_BEFORE:BEGIN===' '===DAY25:IRQ_INFO_BEFORE:END===' "${out}/irq-info-before.txt"
extract_block '===DAY25:IRQ_COUNT_BEFORE:BEGIN===' '===DAY25:IRQ_COUNT_BEFORE:END===' "${out}/irq-count-before.txt"
extract_block '===DAY25:IRQ_STATUS_BEFORE:BEGIN===' '===DAY25:IRQ_STATUS_BEFORE:END===' "${out}/irq-status-before.txt"
extract_block '===DAY25:PROC_INTERRUPTS_BEFORE:BEGIN===' '===DAY25:PROC_INTERRUPTS_BEFORE:END===' "${out}/proc-interrupts-before.txt"
extract_block '===DAY25:TRIGGER:BEGIN===' '===DAY25:TRIGGER:END===' "${out}/trigger.txt"
extract_block '===DAY25:IRQ_COUNT_AFTER:BEGIN===' '===DAY25:IRQ_COUNT_AFTER:END===' "${out}/irq-count-after.txt"
extract_block '===DAY25:IRQ_STATUS_AFTER:BEGIN===' '===DAY25:IRQ_STATUS_AFTER:END===' "${out}/irq-status-after.txt"
extract_block '===DAY25:PROC_INTERRUPTS_AFTER:BEGIN===' '===DAY25:PROC_INTERRUPTS_AFTER:END===' "${out}/proc-interrupts-after.txt"
extract_block '===DAY25:DMESG_DRIVER:BEGIN===' '===DAY25:DMESG_DRIVER:END===' "${out}/dmesg-driver.txt"

# 生成一份简洁摘要。这里保持“事实汇总”，不强行把结果包装成全部通过；
# 若某项没增长，就明确写 no，方便后续复盘。
{
    echo "# day25 run summary"
    echo
    if grep -q '1234:11e8' "${out}/lspci-nn.txt"; then echo "- edu device visible: yes"; else echo "- edu device visible: no"; fi
    if grep -q 'probe success' "${out}/dmesg-driver.txt"; then echo "- probe success: yes"; else echo "- probe success: no"; fi
    if grep -qi 'BAR0:' "${out}/dmesg-driver.txt"; then echo "- BAR0 logged: yes"; else echo "- BAR0 logged: no"; fi
    before=$(grep -o '[0-9][0-9]*' "${out}/irq-count-before.txt" | tail -n1 || echo 0)
    after=$(grep -o '[0-9][0-9]*' "${out}/irq-count-after.txt" | tail -n1 || echo 0)
    if [ ${after:-0} -gt ${before:-0} ]; then echo "- driver irq_count grows: yes"; else echo "- driver irq_count grows: no"; fi
    if grep -q 'day25_edu_irq' "${out}/proc-interrupts-after.txt"; then echo "- /proc/interrupts entry exists: yes"; else echo "- /proc/interrupts entry exists: no"; fi
    p_before=$(awk '/day25_edu_irq/ {print $2; exit}' "${out}/proc-interrupts-before.txt" 2>/dev/null || echo 0)
    p_after=$(awk '/day25_edu_irq/ {print $2; exit}' "${out}/proc-interrupts-after.txt" 2>/dev/null || echo 0)
    if [ ${p_after:-0} -gt ${p_before:-0} ]; then echo "- /proc/interrupts count grows: yes"; else echo "- /proc/interrupts count grows: no"; fi
    if grep -q '===DAY25:COMPLETE===' "$serial"; then echo "- guest flow complete: yes"; else echo "- guest flow complete: no"; fi
  } > "${out}/run-summary.md"

echo "[day25] records 已生成：${out}"
