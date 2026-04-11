#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
ensure_run_dir

SERIAL_LOG="${WORKDIR}/runs/${RUN_ID}/serial.log"
QEMU_ERR="${WORKDIR}/runs/${RUN_ID}/qemu.stderr.log"
QEMU_CMD="${WORKDIR}/runs/${RUN_ID}/qemu-command.txt"
RECORD_DIR="${DAY23_ROOT}/records/${RUN_ID}"

require_file SERIAL_LOG "${SERIAL_LOG}"
rm -rf "${RECORD_DIR}"
mkdir -p "${RECORD_DIR}"
cp "${SERIAL_LOG}" "${RECORD_DIR}/serial.log"
[[ -f "${QEMU_ERR}" ]] && cp "${QEMU_ERR}" "${RECORD_DIR}/qemu.stderr.log"
[[ -f "${QEMU_CMD}" ]] && cp "${QEMU_CMD}" "${RECORD_DIR}/qemu-command.txt"

extract_between_markers '===DAY23:LSPCI_NN:BEGIN===' '===DAY23:LSPCI_NN:END===' "${SERIAL_LOG}" "${RECORD_DIR}/lspci-nn.txt"
extract_between_markers '===DAY23:LSPCI_VV_NN:BEGIN===' '===DAY23:LSPCI_VV_NN:END===' "${SERIAL_LOG}" "${RECORD_DIR}/lspci-vv-nn.txt"
extract_between_markers '===DAY23:DMESG_PROBE:BEGIN===' '===DAY23:DMESG_PROBE:END===' "${SERIAL_LOG}" "${RECORD_DIR}/dmesg-probe.txt"
extract_between_markers '===DAY23:DMESG_REMOVE:BEGIN===' '===DAY23:DMESG_REMOVE:END===' "${SERIAL_LOG}" "${RECORD_DIR}/dmesg-remove.txt"
extract_between_markers '===DAY23:SYSFS_DEVICES:BEGIN===' '===DAY23:SYSFS_DEVICES:END===' "${SERIAL_LOG}" "${RECORD_DIR}/sysfs-pci-devices.txt"

insmod_ok=no
rmmod_ok=no
probe_ok=no
bar0_ok=no
bar2_ok=no
complete=no

if grep -q '===DAY23:INSMOD:OK===' "${SERIAL_LOG}"; then insmod_ok=yes; fi
if grep -q '===DAY23:RMMOD:OK===' "${SERIAL_LOG}"; then rmmod_ok=yes; fi
if grep -q 'probe success' "${RECORD_DIR}/dmesg-probe.txt" 2>/dev/null; then probe_ok=yes; fi
if grep -q 'BAR0:' "${RECORD_DIR}/dmesg-probe.txt" 2>/dev/null; then bar0_ok=yes; fi
if grep -q 'BAR2:' "${RECORD_DIR}/dmesg-probe.txt" 2>/dev/null; then bar2_ok=yes; fi
if grep -q '===DAY23:COMPLETE===' "${SERIAL_LOG}"; then complete=yes; fi

cat > "${RECORD_DIR}/run-summary.md" <<EOF
# day23 run summary

- RUN_ID: ${RUN_ID}
- insmod 成功：${insmod_ok}
- probe 成功：${probe_ok}
- BAR0 信息：${bar0_ok}
- BAR2 信息：${bar2_ok}
- rmmod 成功：${rmmod_ok}
- guest 流程完成：${complete}

## 建议优先查看

- serial.log
- dmesg-probe.txt
- dmesg-remove.txt
- lspci-vv-nn.txt
EOF

log "records 已生成：${RECORD_DIR}"
