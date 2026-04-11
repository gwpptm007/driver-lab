#!/usr/bin/env bash
set -euo pipefail

DAY23_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "${DAY23_ROOT}/env/day23.env"

mkdir -p "${WORKDIR}" "${WORKDIR}/runs/${RUN_ID}"

log() {
    echo "[day23] $*"
}

warn() {
    echo "[day23][WARN] $*" >&2
}

die() {
    echo "[day23][ERROR] $*" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || die "缺少命令：$1"
}

require_file() {
    local var_name="$1"
    local path="$2"
    [[ -n "${path}" ]] || die "变量 ${var_name} 未设置"
    [[ -e "${path}" ]] || die "${var_name} 指向的文件不存在：${path}"
}

ensure_run_dir() {
    mkdir -p "${WORKDIR}/runs/${RUN_ID}"
}

extract_between_markers() {
    local begin="$1" end="$2" src="$3" dst="$4"
    awk -v b="$begin" -v e="$end" '
        $0 ~ b {inblock=1; next}
        $0 ~ e {inblock=0; exit}
        inblock {print}
    ' "$src" > "$dst"
}
