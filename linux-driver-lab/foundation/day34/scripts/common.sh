#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/../env/day34.env"

require_file() {
    local path="${1:-}"
    local name="${2:-$1}"
    if [ -z "$path" ]; then
        echo "[day34][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi
    if [ ! -e "$path" ]; then
        echo "[day34][ERROR] 文件不存在：$path" >&2
        exit 1
    fi
}

require_exec() {
    local prog="${1:-}"
    local name="${2:-$1}"
    if [ -z "$prog" ]; then
        echo "[day34][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi
    case "$prog" in
        */*) [ -x "$prog" ] || { echo "[day34][ERROR] 可执行文件不存在或不可执行：$prog" >&2; exit 1; } ;;
        *) command -v "$prog" >/dev/null 2>&1 || { echo "[day34][ERROR] 命令不存在或不可执行：$prog" >&2; exit 1; } ;;
    esac
}

ensure_dir() { mkdir -p "$1"; }
run_dir() { echo "${WORKDIR}/runs/${RUN_ID}"; }
marker_extract() {
    local marker="$1" src="$2" dst="$3"
    tr -d '' < "$src" | awk "/===${marker}:BEGIN===/{flag=1;next}/===${marker}:END===/{flag=0}flag" > "$dst" || true
}
