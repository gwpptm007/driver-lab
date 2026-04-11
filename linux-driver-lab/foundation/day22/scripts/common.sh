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


auto_pick_first() {
    local pattern="$1"
    find "${REPO_ROOT}" -path "$pattern" 2>/dev/null | sort | head -n1
}

auto_pick_busybox() {
    while IFS= read -r p; do
        [[ -z "$p" ]] && continue
        if ${FILE_BIN} "$p" 2>/dev/null | ${GREP_BIN} -qi 'ARM aarch64\|ARM64'; then
            echo "$p"
            return 0
        fi
    done < <(find "${REPO_ROOT}" -type f -name busybox 2>/dev/null | sort)
    return 1
}

auto_pick_lspci() {
    while IFS= read -r p; do
        [[ -z "$p" ]] && continue
        if ${FILE_BIN} "$p" 2>/dev/null | ${GREP_BIN} -qi 'ARM aarch64\|ARM64'; then
            echo "$p"
            return 0
        fi
    done < <(find "${REPO_ROOT}" -type f -name lspci 2>/dev/null | sort)
    return 1
}

auto_pick_pciutils_src() {
    while IFS= read -r p; do
        [[ -z "$p" ]] && continue
        if [[ -f "$p/Makefile" && -f "$p/lspci.c" ]]; then
            echo "$p"
            return 0
        fi
    done < <(find "${REPO_ROOT}" -type d -name pciutils 2>/dev/null | sort)
    return 1
}

auto_fill_platform_paths() {
    local changed=0
    if [[ -z "${KERNEL_IMAGE:-}" ]]; then
        local v
        v="$(auto_pick_first '*/linux-*/build/arm64/arch/arm64/boot/Image' || true)"
        if [[ -n "$v" ]]; then
            export KERNEL_IMAGE="$v"
            changed=1
        fi
    fi
    if [[ -z "${KERNEL_CONFIG_PATH:-}" ]]; then
        local v
        v="$(auto_pick_first '*/linux-*/build/arm64/.config' || true)"
        if [[ -n "$v" ]]; then
            export KERNEL_CONFIG_PATH="$v"
            changed=1
        fi
    fi
    if [[ -z "${BUSYBOX_BIN:-}" ]]; then
        local v
        v="$(auto_pick_busybox || true)"
        if [[ -n "$v" ]]; then
            export BUSYBOX_BIN="$v"
            changed=1
        fi
    fi
    if [[ -z "${PCIUTILS_SRC_DIR:-}" || ! -d "${PCIUTILS_SRC_DIR}" ]]; then
        local v
        v="$(auto_pick_pciutils_src || true)"
        if [[ -n "$v" ]]; then
            export PCIUTILS_SRC_DIR="$v"
            changed=1
        fi
    fi
    if [[ -z "${GUEST_LSPCI_BIN:-}" || "${GUEST_LSPCI_BIN}" == "${TOOLS_DIR}/aarch64/lspci" || ! -x "${GUEST_LSPCI_BIN}" ]]; then
        local v
        if [[ -n "${PCIUTILS_SRC_DIR:-}" && -x "${PCIUTILS_SRC_DIR}/lspci" ]]; then
            v="${PCIUTILS_SRC_DIR}/lspci"
        else
            v="$(auto_pick_lspci || true)"
        fi
        if [[ -n "$v" ]]; then
            export GUEST_LSPCI_BIN="$v"
            changed=1
        fi
    fi
    if [[ $changed -eq 1 ]]; then
        log "已根据当前仓库自动补全部分路径；建议执行 make discover-paths 查看完整推荐值。"
    fi
}

require_nonempty_var() {
    local name="$1"
    local value="${2:-}"
    [[ -n "$value" ]] || die "变量 ${name} 未设置；先执行 make discover-paths 并 export，或查看 docs/02_PREPARE_ENV_AND_LSPCI.md"
}

require_file_named() {
    local name="$1"
    local path="$2"
    require_nonempty_var "$name" "$path"
    [[ -f "$path" ]] || die "${name} 指向的文件不存在：${path}"
}

require_executable_named() {
    local name="$1"
    local path="$2"
    require_nonempty_var "$name" "$path"
    [[ -x "$path" ]] || die "${name} 指向的可执行文件不存在或不可执行：${path}"
}

is_elf_aarch64_static() {
    local path="$1"
    local out
    out="$(${FILE_BIN} "$path" 2>/dev/null || true)"
    [[ "$out" == *"ARM aarch64"* || "$out" == *"ARM64"* ]] || return 1
    [[ "$out" == *"statically linked"* ]] || return 1
    return 0
}
