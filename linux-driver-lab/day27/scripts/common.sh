#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/../env/day27.env"

# day27 所有脚本共享的小工具函数都收在这里，避免每个脚本重复写一遍。

require_file() {
    local path="${1:-}"
    local name="${2:-$1}"
    if [ -z "$path" ]; then
        echo "[day27][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi
    if [ ! -e "$path" ]; then
        echo "[day27][ERROR] 文件不存在：$path" >&2
        exit 1
    fi
}

require_exec() {
    local path="${1:-}"
    local name="${2:-$1}"
    if [ -z "$path" ]; then
        echo "[day27][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi
    if [ ! -x "$path" ]; then
        echo "[day27][ERROR] 可执行文件不存在或不可执行：$path" >&2
        exit 1
    fi
}

# 确保目录存在。
ensure_dir() { mkdir -p "$1"; }

# 每次 run 的临时目录统一放在 workdir/runs/<RUN_ID>/ 下。
run_dir() { echo "${WORKDIR}/runs/${RUN_ID}"; }

# 从完整串口日志里切出 BEGIN/END marker 中间的内容。
marker_extract() {
    local marker="$1" src="$2" dst="$3"
    awk "/===${marker}:BEGIN===/{flag=1;next}/===${marker}:END===/{flag=0}flag" "$src" > "$dst" || true
}
