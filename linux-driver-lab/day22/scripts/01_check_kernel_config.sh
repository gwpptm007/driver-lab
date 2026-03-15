#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

# day22 的目的不是替你编译内核，而是尽早发现“平台能力压根没打开”的问题。
# 所以这里只做检查，不做自动改 config。

if [[ -z "${KERNEL_CONFIG_PATH}" ]]; then
    warn "未设置 KERNEL_CONFIG_PATH，跳过内核 PCI 配置检查。"
    exit 0
fi

require_file "${KERNEL_CONFIG_PATH}"
ensure_dir "${RUNS_DIR}/${RUN_ID}"
report="${RUNS_DIR}/${RUN_ID}/kernel-config-check.txt"

log "检查内核 PCI/MSI 关键配置：${KERNEL_CONFIG_PATH}"
{
    echo "# day22 kernel config check"
    echo "# file: ${KERNEL_CONFIG_PATH}"
    echo

    check_yes() {
        local key="$1"
        if ${GREP_BIN} -q "^${key}=y$" "${KERNEL_CONFIG_PATH}"; then
            echo "[OK] ${key}=y"
        elif ${GREP_BIN} -q "^# ${key} is not set$" "${KERNEL_CONFIG_PATH}"; then
            echo "[FAIL] ${key} is not set"
        else
            echo "[WARN] ${key} not found as explicit =y"
        fi
    }

    check_yes CONFIG_PCI
    check_yes CONFIG_PCI_MSI
    check_yes CONFIG_PCI_HOST_GENERIC
    check_yes CONFIG_PCI_DOMAINS
    check_yes CONFIG_VFIO || true
    echo
    echo "提示：day22 只要求 PCI 设备能被枚举到。"
    echo "其中最关键的是 CONFIG_PCI / CONFIG_PCI_MSI / CONFIG_PCI_HOST_GENERIC。"
} | tee "${report}"

log "内核配置检查结果已写入：${report}"
