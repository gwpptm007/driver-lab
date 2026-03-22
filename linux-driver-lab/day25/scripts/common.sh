#!/usr/bin/env bash
set -euo pipefail

# 通用辅助函数：
# - require_file / require_exec：统一的输入检查
# - ensure_dir：确保目录存在
# - run_dir：根据 RUN_ID 生成本次运行目录

require_file() {
    local path="$1"
    local name="${2:-$1}"
    if [ -z "$path" ]; then
        echo "[day25][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi
    if [ ! -e "$path" ]; then
        echo "[day25][ERROR] 文件不存在：$path" >&2
        exit 1
    fi
}

require_exec() {
    local path="$1"
    local name="${2:-$1}"
    if [ -z "$path" ]; then
        echo "[day25][ERROR] 变量 ${name} 未设置" >&2
        exit 1
    fi
    if [ ! -x "$path" ]; then
        echo "[day25][ERROR] 可执行文件不存在或不可执行：$path" >&2
        exit 1
    fi
}

ensure_dir() {
    mkdir -p "$1"
}

run_dir() {
    echo "${WORKDIR}/runs/${RUN_ID}"
}
