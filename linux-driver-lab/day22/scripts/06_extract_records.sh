#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

RUN_DIR="${RUNS_DIR}/${RUN_ID}"
if [[ ! -d "${RUN_DIR}" ]]; then
    latest="$(latest_run_dir || true)"
    [[ -n "${latest}" ]] || die "找不到任何 run 目录，无法提取 records。"
    RUN_DIR="${latest}"
fi

SERIAL_LOG="${RUN_DIR}/serial.log"
require_file "${SERIAL_LOG}"

RECORD_DIR="${DAY22_ROOT}/records/$(basename "${RUN_DIR}")"
ensure_dir "${RECORD_DIR}"

# 切分串口日志中的关键片段。guest init 脚本会打印成对标记，便于主机侧抽取。
marker_extract "${SERIAL_LOG}" '===DAY22:LSPCI_NN:BEGIN===' '===DAY22:LSPCI_NN:END===' "${RECORD_DIR}/lspci-nn.txt"
marker_extract "${SERIAL_LOG}" '===DAY22:LSPCI_VV_NN:BEGIN===' '===DAY22:LSPCI_VV_NN:END===' "${RECORD_DIR}/lspci-vv-nn.txt"
marker_extract "${SERIAL_LOG}" '===DAY22:DMESG_PCI:BEGIN===' '===DAY22:DMESG_PCI:END===' "${RECORD_DIR}/dmesg-pci.txt"
marker_extract "${SERIAL_LOG}" '===DAY22:SYSFS_PCI_DEVICES:BEGIN===' '===DAY22:SYSFS_PCI_DEVICES:END===' "${RECORD_DIR}/sysfs-pci-devices.txt"
marker_extract "${SERIAL_LOG}" '===DAY22:PCI_CONFIG_DUMP:BEGIN===' '===DAY22:PCI_CONFIG_DUMP:END===' "${RECORD_DIR}/pci-config-dump.txt"

# 原始日志与辅助日志全部拷贝保留，避免后面只剩“切分后的结论”。
for name in serial.log qemu.stderr.log qemu.stdout.log qemu-command.txt server.log kernel-config-check.txt; do
    if [[ -f "${RUN_DIR}/${name}" ]]; then
        cp -f "${RUN_DIR}/${name}" "${RECORD_DIR}/${name}"
    fi
done

# 自动写一份简要结论，方便你当场判断 day22 是不是通过。
{
    echo "# day22 run summary"
    echo
    echo "- run-id: $(basename "${RUN_DIR}")"
    echo "- record-dir: ${RECORD_DIR}"
    echo
    if ${GREP_BIN} -q '1af4:1110' "${RECORD_DIR}/lspci-nn.txt" 2>/dev/null; then
        echo "- ivshmem 设备可见：是（在 lspci -nn 中发现 1af4:1110）"
    else
        echo "- ivshmem 设备可见：否（未在 lspci -nn 中发现 1af4:1110）"
    fi
    echo "- lspci -vv 归档：$( [[ -s "${RECORD_DIR}/lspci-vv-nn.txt" ]] && echo 是 || echo 否 )"
    echo "- dmesg PCI 归档：$( [[ -s "${RECORD_DIR}/dmesg-pci.txt" ]] && echo 是 || echo 否 )"
    echo "- sysfs PCI 设备归档：$( [[ -s "${RECORD_DIR}/sysfs-pci-devices.txt" ]] && echo 是 || echo 否 )"
    echo
    echo "建议下一步："
    echo "1. 打开 lspci-vv-nn.txt，看 BAR / Capabilities 是否已能看到基本信息。"
    echo "2. day23 基于该 BDF 和设备 ID 开始写 pci_driver 骨架。"
} > "${RECORD_DIR}/run-summary.md"

log "records 已生成：${RECORD_DIR}"
