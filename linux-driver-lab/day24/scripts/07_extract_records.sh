#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
ensure_run_dir

SERIAL_LOG="${WORKDIR}/runs/${RUN_ID}/serial.log"
QEMU_ERR="${WORKDIR}/runs/${RUN_ID}/qemu.stderr.log"
QEMU_CMD="${WORKDIR}/runs/${RUN_ID}/qemu-command.txt"
RECORD_DIR="${DAY24_ROOT}/records/${RUN_ID}"

require_file SERIAL_LOG "${SERIAL_LOG}"
rm -rf "${RECORD_DIR}"
mkdir -p "${RECORD_DIR}"
cp "${SERIAL_LOG}" "${RECORD_DIR}/serial.log"
[[ -f "${QEMU_ERR}" ]] && cp "${QEMU_ERR}" "${RECORD_DIR}/qemu.stderr.log"
[[ -f "${QEMU_CMD}" ]] && cp "${QEMU_CMD}" "${RECORD_DIR}/qemu-command.txt"

extract_between_markers '===DAY24:LSPCI_NN:BEGIN===' '===DAY24:LSPCI_NN:END===' "${SERIAL_LOG}" "${RECORD_DIR}/lspci-nn.txt"
extract_between_markers '===DAY24:LSPCI_VV_NN:BEGIN===' '===DAY24:LSPCI_VV_NN:END===' "${SERIAL_LOG}" "${RECORD_DIR}/lspci-vv-nn.txt"
extract_between_markers '===DAY24:MMIO_INFO:BEGIN===' '===DAY24:MMIO_INFO:END===' "${SERIAL_LOG}" "${RECORD_DIR}/mmio-info.txt"
extract_between_markers '===DAY24:MMIO_READ_BEFORE:BEGIN===' '===DAY24:MMIO_READ_BEFORE:END===' "${SERIAL_LOG}" "${RECORD_DIR}/mmio-read-before.txt"
extract_between_markers '===DAY24:MMIO_WRITE_STATE:BEGIN===' '===DAY24:MMIO_WRITE_STATE:END===' "${SERIAL_LOG}" "${RECORD_DIR}/mmio-write-state.txt"
extract_between_markers '===DAY24:MMIO_READ_AFTER:BEGIN===' '===DAY24:MMIO_READ_AFTER:END===' "${SERIAL_LOG}" "${RECORD_DIR}/mmio-read-after.txt"
extract_between_markers '===DAY24:SHM_WRITE:BEGIN===' '===DAY24:SHM_WRITE:END===' "${SERIAL_LOG}" "${RECORD_DIR}/shm-write.txt"
extract_between_markers '===DAY24:SHM_READ:BEGIN===' '===DAY24:SHM_READ:END===' "${SERIAL_LOG}" "${RECORD_DIR}/shm-read.txt"
extract_between_markers '===DAY24:DMESG_DRIVER:BEGIN===' '===DAY24:DMESG_DRIVER:END===' "${SERIAL_LOG}" "${RECORD_DIR}/dmesg-driver.txt"

insmod_ok=no
probe_ok=no
mmio_info_ok=no
mmio_write_ok=no
mmio_read_after_ok=no
shm_write_ok=no
shm_read_ok=no
rmmod_ok=no
complete=no

if grep -q '===DAY24:INSMOD:OK===' "${SERIAL_LOG}"; then insmod_ok=yes; fi
if grep -q 'probe success' "${RECORD_DIR}/dmesg-driver.txt" 2>/dev/null; then probe_ok=yes; fi
if grep -q 'proto_magic=' "${RECORD_DIR}/mmio-info.txt" 2>/dev/null; then mmio_info_ok=yes; fi
if grep -q 'mmio-write ok' "${RECORD_DIR}/mmio-write-state.txt" 2>/dev/null; then mmio_write_ok=yes; fi
if grep -q 'offset=0x0000000c' "${RECORD_DIR}/mmio-read-after.txt" 2>/dev/null; then mmio_read_after_ok=yes; fi
if grep -q 'shm-write ok' "${RECORD_DIR}/shm-write.txt" 2>/dev/null; then shm_write_ok=yes; fi
if grep -q "${DAY24_EXPECTED_PAYLOAD}" "${RECORD_DIR}/shm-read.txt" 2>/dev/null; then shm_read_ok=yes; fi
if grep -q '===DAY24:RMMOD:OK===' "${SERIAL_LOG}"; then rmmod_ok=yes; fi
if grep -q '===DAY24:COMPLETE===' "${SERIAL_LOG}"; then complete=yes; fi

cat > "${RECORD_DIR}/run-summary.md" <<EOT
# day24 run summary

- RUN_ID: ${RUN_ID}
- insmod 成功：${insmod_ok}
- probe 成功：${probe_ok}
- mmio info：${mmio_info_ok}
- mmio write：${mmio_write_ok}
- mmio read after：${mmio_read_after_ok}
- shm write：${shm_write_ok}
- shm read：${shm_read_ok}
- rmmod 成功：${rmmod_ok}
- guest 流程完成：${complete}

## 建议优先查看

- serial.log
- dmesg-driver.txt
- mmio-info.txt
- shm-read.txt
- lspci-vv-nn.txt
EOT

log "records 已生成：${RECORD_DIR}"
