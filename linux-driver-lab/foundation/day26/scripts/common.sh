#!/usr/bin/env bash
set -euo pipefail

# 统一加载 day26 环境变量。所有脚本都从这里拿：
# - DAY26_ROOT / WORKDIR / ROOTFS_IMG / RECORDS_DIR
# - RUN_ID / QEMU_BIN / ARCH / CROSS_COMPILE
# - PCIUTILS_SRC_DIR / GUEST_LSPCI_BIN
source "$(dirname "$0")/../env/day26.env"

# 检查“文件存在”。适用于：Image、.config、BusyBox、KDIR 等。
require_file() {
    local path="${1:-}"
    local name="${2:-$1}"
    if [ -z "$path" ]; then
        echo "[day26][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi
    if [ ! -e "$path" ]; then
        echo "[day26][ERROR] 文件不存在：$path" >&2
        exit 1
    fi
}

# 检查“可执行文件存在”。适用于：BusyBox、lspci、guest tool。
require_exec() {
    local path="${1:-}"
    local name="${2:-$1}"
    if [ -z "$path" ]; then
        echo "[day26][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi
    if [ ! -x "$path" ]; then
        echo "[day26][ERROR] 可执行文件不存在或不可执行：$path" >&2
        exit 1
    fi
}

ensure_dir() {
    mkdir -p "$1"
}

# 每次运行的临时目录：workdir/runs/<RUN_ID>/
run_dir() {
    echo "${WORKDIR}/runs/${RUN_ID}"
}
