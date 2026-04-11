#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/../env/day30.env"

# common.sh 只放“被多个脚本复用的公共能力”，例如：
# - 路径/命令检查
# - 运行目录计算
# - marker 切块
# 这样具体脚本就能把精力放在本阶段业务上。

require_file() {
    local path="${1:-}"
    local name="${2:-$1}"
    if [ -z "$path" ]; then
        echo "[day30][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi
    if [ ! -e "$path" ]; then
        echo "[day30][ERROR] 文件不存在：$path" >&2
        exit 1
    fi
}

require_exec() {
    local prog="${1:-}"
    local name="${2:-$1}"

    if [ -z "$prog" ]; then
        echo "[day30][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi

    case "$prog" in
        */*)
            if [ ! -x "$prog" ]; then
                echo "[day30][ERROR] 可执行文件不存在或不可执行：$prog" >&2
                exit 1
            fi
            ;;
        *)
            if ! command -v "$prog" >/dev/null 2>&1; then
                echo "[day30][ERROR] 命令不存在或不可执行：$prog" >&2
                exit 1
            fi
            ;;
    esac
}

ensure_dir() { mkdir -p "$1"; }
run_dir() { echo "${WORKDIR}/runs/${RUN_ID}"; }

marker_extract() {
    local marker="$1" src="$2" dst="$3"
    awk "/===${marker}:BEGIN===/{flag=1;next}/===${marker}:END===/{flag=0}flag" "$src" > "$dst" || true
}
