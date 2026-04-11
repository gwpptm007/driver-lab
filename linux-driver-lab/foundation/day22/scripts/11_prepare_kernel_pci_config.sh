#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/common.sh"

# 这个脚本只负责“帮用户把 arm64 内核的 PCI 相关配置打开并说明下一步怎么编译”，
# 不会偷偷替用户执行 make Image。这样既把步骤落地，又不越过“内核编译相关除外”的边界。

is_kernel_source_root() {
    local d="$1"
    [[ -n "$d" && -f "$d/Makefile" && -x "$d/scripts/config" ]]
}

infer_kernel_tree_from_kdir() {
    local d="$1"
    if [[ -z "$d" ]]; then
        return 1
    fi
    d="$(cd "$d" && pwd)"
    while [[ "$d" != "/" ]]; do
        if is_kernel_source_root "$d"; then
            printf '%s\n' "$d"
            return 0
        fi
        d="$(dirname "$d")"
    done
    return 1
}

KERNEL_BUILD_DIR="${KERNEL_BUILD_DIR:-${KDIR:-}}"
KERNEL_SRC_ROOT="${KERNEL_SRC_ROOT:-}"

if [[ -z "${KERNEL_SRC_ROOT}" ]]; then
    KERNEL_SRC_ROOT="$(infer_kernel_tree_from_kdir "${KERNEL_BUILD_DIR}")" || true
fi

[[ -n "${KERNEL_BUILD_DIR}" ]] || die "请先 export KDIR=/path/to/kernel-build-dir，例如 build/arm64"
[[ -n "${KERNEL_SRC_ROOT}" ]] || die "无法根据 KDIR 推断内核源码根目录；请显式 export KERNEL_SRC_ROOT=/path/to/linux-src"
[[ -d "${KERNEL_BUILD_DIR}" ]] || die "KDIR 不存在：${KERNEL_BUILD_DIR}"
[[ -d "${KERNEL_SRC_ROOT}" ]] || die "KERNEL_SRC_ROOT 不存在：${KERNEL_SRC_ROOT}"
is_kernel_source_root "${KERNEL_SRC_ROOT}" || die "KERNEL_SRC_ROOT 看起来不是有效的内核源码根目录（缺 Makefile 或 scripts/config）：${KERNEL_SRC_ROOT}"
[[ -f "${KERNEL_BUILD_DIR}/.config" ]] || die "build 目录里没有 .config：${KERNEL_BUILD_DIR}/.config"

log "准备把 arm64 内核的 PCI 关键配置打开"
print_kv "KERNEL_SRC_ROOT" "${KERNEL_SRC_ROOT}"
print_kv "KERNEL_BUILD_DIR" "${KERNEL_BUILD_DIR}"
print_kv ".config" "${KERNEL_BUILD_DIR}/.config"

"${KERNEL_SRC_ROOT}/scripts/config" --file "${KERNEL_BUILD_DIR}/.config" \
  -e PCI \
  -e PCI_MSI \
  -e PCI_HOST_GENERIC \
  -e PCI_DOMAINS_GENERIC \
  -e PCIEPORTBUS

log "已修改 .config，接下来用 olddefconfig 补齐依赖"
make -C "${KERNEL_SRC_ROOT}" ARCH=arm64 O="${KERNEL_BUILD_DIR}" olddefconfig

report_dir="${RUNS_DIR}/${RUN_ID}"
ensure_dir "${report_dir}"
report="${report_dir}/kernel-pci-prep.txt"

{
    echo "# day22 kernel PCI prep"
    echo "# source root : ${KERNEL_SRC_ROOT}"
    echo "# build dir   : ${KERNEL_BUILD_DIR}"
    echo
    grep -E '^(CONFIG_PCI|CONFIG_PCI_MSI|CONFIG_PCI_HOST_GENERIC|CONFIG_PCI_DOMAINS_GENERIC|CONFIG_PCIEPORTBUS)=' "${KERNEL_BUILD_DIR}/.config" || true
    echo
    echo "下一步："
    echo "  make -C ${KERNEL_SRC_ROOT} -j\"\$(nproc)\" ARCH=arm64 O=${KERNEL_BUILD_DIR} Image"
    echo "然后回到 day22 目录重新执行："
    echo "  make check"
    echo "  make build-tools"
    echo "  make selftest-tool"
    echo "  make rootfs"
    echo "  make backend"
    echo "  make module"
    echo "  make run"
} | tee "${report}"

log "内核 PCI 准备结果已写入：${report}"
