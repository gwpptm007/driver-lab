#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

log "扫描当前仓库常见的 arm64 平台产物路径"
print_kv "REPO_ROOT" "${REPO_ROOT}"

IMAGE_PATH="$(auto_pick_first '*/linux-*/build/arm64/arch/arm64/boot/Image' || true)"
CONFIG_PATH="$(auto_pick_first '*/linux-*/build/arm64/.config' || true)"
BUSYBOX_PATH="$(auto_pick_busybox || true)"
LSPCI_PATH="$(auto_pick_lspci || true)"
KDIR_PATH="$(auto_pick_first '*/linux-*/build/arm64' || true)"

print_kv "Image 候选" "${IMAGE_PATH:-<未找到>}"
print_kv ".config 候选" "${CONFIG_PATH:-<未找到>}"
print_kv "BusyBox 候选" "${BUSYBOX_PATH:-<未找到>}"
print_kv "lspci 候选" "${LSPCI_PATH:-<未找到>}"
print_kv "KDIR 候选" "${KDIR_PATH:-<未找到>}"

echo
cat <<ENVOUT
# 复制下面这组命令到当前 shell：
export RUN_ID=day22-local-001
export KERNEL_IMAGE=${IMAGE_PATH:-/请手动填写/Image}
export KERNEL_CONFIG_PATH=${CONFIG_PATH:-/请手动填写/.config}
export BUSYBOX_BIN=${BUSYBOX_PATH:-/请手动填写/busybox}
export GUEST_LSPCI_BIN=${LSPCI_PATH:-/请手动填写/lspci}
export KDIR=${KDIR_PATH:-/请手动填写/linux-build-dir}
ENVOUT

echo
if [[ -z "${LSPCI_PATH}" ]]; then
    warn "当前仓库里没有发现 arm64 lspci。"
    warn "day22 的 lspci 验收无法跳过；请准备一个 arm64 静态 lspci，然后 export GUEST_LSPCI_BIN=..."
    warn "如果你想自己构建，请先把 pciutils 源码放到 ${PCIUTILS_SRC_DIR}，再执行 make build-lspci。"
    warn "详细流程见：docs/02_PREPARE_ENV_AND_LSPCI.md"
fi
