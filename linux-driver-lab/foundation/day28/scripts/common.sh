#!/usr/bin/env bash
# day28 的公共脚本函数。
#
# 这个文件只提供最基础的工具：
# - 统一定位 day28/day22~day27 目录
# - 统一错误输出
# - 统一创建输出目录
#
# 注意：day28 本身不再构建内核、不再启动 QEMU，它只消费已经存在的 records。

set -euo pipefail

DAY28_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LAB_ROOT="$(cd "${DAY28_ROOT}/.." && pwd)"
OUTPUT_DIR="${DAY28_ROOT}/output"
SNAPSHOT_DIR="${DAY28_ROOT}/records/w4-evidence-snapshot"

log() {
    echo "[day28] $*"
}

warn() {
    echo "[day28][WARN] $*" >&2
}

die() {
    echo "[day28][ERROR] $*" >&2
    exit 1
}

ensure_dir() {
    mkdir -p "$1"
}

require_dir() {
    local path="$1"
    local name="${2:-$1}"
    [[ -d "$path" ]] || die "目录不存在：${name} -> ${path}"
}
