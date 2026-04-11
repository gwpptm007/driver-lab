#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
ensure_run_dir

require_file KERNEL_SRC_ROOT "${KERNEL_SRC_ROOT}/Makefile"
require_file KDIR "${KDIR}"

log "准备 day24 外部模块构建所需的内核模块树"

make -C "${KERNEL_SRC_ROOT}" \
    ARCH="${ARCH}" \
    CROSS_COMPILE="${CROSS_COMPILE}" \
    O="${KDIR}" \
    modules_prepare

if [[ ! -f "${KDIR}/Module.symvers" ]] || ! grep -q '__pci_register_driver' "${KDIR}/Module.symvers"; then
    warn "当前 Module.symvers 缺少 PCI driver 注册相关导出符号，继续补一轮 modules。"
    make -C "${KERNEL_SRC_ROOT}" \
        -j"$(nproc)" \
        ARCH="${ARCH}" \
        CROSS_COMPILE="${CROSS_COMPILE}" \
        O="${KDIR}" \
        modules
fi

outfile="${WORKDIR}/runs/${RUN_ID}/kernel-module-tree.txt"
{
    echo "# day24 kernel module tree check"
    echo "# source root : ${KERNEL_SRC_ROOT}"
    echo "# build dir   : ${KDIR}"
    echo
    ls -l "${KDIR}/Module.symvers" 2>/dev/null || true
    grep -E '(__pci_register_driver|pci_unregister_driver|pci_enable_device|pci_iomap)' "${KDIR}/Module.symvers" 2>/dev/null || true
} > "${outfile}"

log "内核模块树准备结果已写入：${outfile}"
