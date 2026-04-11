#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

ensure_dir "$(run_dir)"

echo "[day26] 开始检查 day26 宿主机环境（EDU + tool + clear errno 版本）"
echo "DAY26_ROOT             : ${DAY26_ROOT}"
echo "WORKDIR                : ${WORKDIR}"
echo "QEMU_BIN               : ${QEMU_BIN}"
echo "KERNEL_IMAGE           : ${KERNEL_IMAGE:-}"
echo "BUSYBOX_BIN            : ${BUSYBOX_BIN:-}"
echo "GUEST_LSPCI_BIN        : ${GUEST_LSPCI_BIN:-}"
echo "KERNEL_CONFIG_PATH     : ${KERNEL_CONFIG_PATH:-}"
echo "KDIR                   : ${KDIR:-}"
echo "EDU_DEVICE_ID_EXPECT   : ${EDU_DEVICE_ID_EXPECT}"

command -v "${QEMU_BIN}" >/dev/null 2>&1 || { echo "[day26][ERROR] 找不到 ${QEMU_BIN}"; exit 1; }
require_file "${KERNEL_IMAGE:-}" KERNEL_IMAGE
require_exec "${BUSYBOX_BIN:-}" BUSYBOX_BIN

# lspci 是 day26 验收的硬前提：没它就无法在 guest 内做 lspci -nn / -vv -nn。
if [ -x "${GUEST_LSPCI_BIN:-}" ]; then
    echo "[day26] 发现可直接使用的 arm64 静态 lspci：${GUEST_LSPCI_BIN}"
else
    echo "[day26][WARN] 当前没有可执行的 GUEST_LSPCI_BIN：${GUEST_LSPCI_BIN:-}"
    echo "[day26][WARN] 请先按 README / RUNBOOK 的主流程准备 third_party/pciutils 再执行 make build-lspci"
fi

# PCI 相关配置不是 day26 代码本身生成的，但它们是本轮实验的外部前置条件。
if [ -n "${KERNEL_CONFIG_PATH:-}" ] && [ -f "${KERNEL_CONFIG_PATH}" ]; then
    echo "[day26] 内核配置文件已指定：${KERNEL_CONFIG_PATH}"
    {
        echo "# day26 kernel config check"
        echo "# file: ${KERNEL_CONFIG_PATH}"
        echo
        for sym in CONFIG_PCI CONFIG_PCI_MSI CONFIG_PCI_HOST_GENERIC CONFIG_VFIO; do
            if grep -q "^${sym}=y" "${KERNEL_CONFIG_PATH}"; then
                echo "[OK] ${sym}=y"
            else
                echo "[WARN] ${sym} not set to y"
            fi
        done
    } | tee "$(run_dir)/kernel-config-check.txt"
else
    echo "[day26][WARN] 未设置 KERNEL_CONFIG_PATH，跳过内核 PCI 配置检查。"
fi
