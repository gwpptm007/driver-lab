#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
ensure_run_dir

[[ -n "${KERNEL_CONFIG_PATH}" && -f "${KERNEL_CONFIG_PATH}" ]] || { warn "未设置 KERNEL_CONFIG_PATH，跳过内核配置检查。"; exit 0; }

outfile="${WORKDIR}/runs/${RUN_ID}/kernel-config-check.txt"
{
    echo "# day24 kernel config check"
    echo "# file: ${KERNEL_CONFIG_PATH}"
    echo
    for sym in CONFIG_PCI CONFIG_PCI_MSI CONFIG_PCI_HOST_GENERIC CONFIG_MODULES CONFIG_MODULE_UNLOAD CONFIG_PCI_DOMAINS CONFIG_VFIO; do
        if grep -q "^${sym}=y" "${KERNEL_CONFIG_PATH}"; then
            echo "[OK] ${sym}=y"
        elif grep -q "^${sym}=" "${KERNEL_CONFIG_PATH}"; then
            echo "[WARN] ${sym} present but not =y"
        else
            echo "[WARN] ${sym} not found as explicit =y"
        fi
    done
    echo
    echo "提示：day24 重点是 MMIO/共享内存协议验证。"
    echo "但前提仍然是 PCI 与模块树已经正确就绪。"
} | tee "${outfile}"

log "内核配置检查结果已写入：${outfile}"
