#!/usr/bin/env bash
set -euo pipefail

# common.sh
#
# day22 的公共函数全部放在这里，避免每个脚本复制一份。

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAY22_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=/dev/null
source "${DAY22_ROOT}/env/day22.env"

log() {
    printf '[day22] %s\n' "$*"
}

warn() {
    printf '[day22][WARN] %s\n' "$*" >&2
}

die() {
    printf '[day22][ERROR] %s\n' "$*" >&2
    exit 1
}

require_cmd() {
    local cmd="$1"
    command -v "$cmd" >/dev/null 2>&1 || die "缺少命令：${cmd}"
}

ensure_dir() {
    mkdir -p "$1"
}

require_file() {
    local path="$1"
    [[ -f "$path" ]] || die "文件不存在：${path}"
}

require_executable_file() {
    local path="$1"
    [[ -x "$path" ]] || die "可执行文件不存在或不可执行：${path}"
}

print_kv() {
    printf '%-24s : %s\n' "$1" "$2"
}

latest_run_dir() {
    if [[ ! -d "${RUNS_DIR}" ]]; then
        return 1
    fi
    ls -1dt "${RUNS_DIR}"/* 2>/dev/null | head -n1
}

marker_extract() {
    local input_file="$1"
    local begin_marker="$2"
    local end_marker="$3"
    local output_file="$4"

    ${AWK_BIN} -v begin="$begin_marker" -v end="$end_marker" '
        $0 == begin { flag=1; next }
        $0 == end   { flag=0; exit }
        flag { print }
    ' "$input_file" > "$output_file"
}

is_elf_aarch64_static() {
    local path="$1"
    local out
    out="$(${FILE_BIN} "$path" 2>/dev/null || true)"
    [[ "$out" == *"ARM aarch64"* || "$out" == *"ARM64"* ]] || return 1
    [[ "$out" == *"statically linked"* ]] || return 1
    return 0
}
