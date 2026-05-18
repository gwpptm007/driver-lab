#!/usr/bin/env bash
set -euo pipefail

LAB_NAME="lab-kprobe-trace-napi-poll"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${LAB_DIR}/records"
REPORT_DIR="${LAB_DIR}/reports"

# 默认观察目标是实验网卡；如果没有流量，可以在运行时覆盖成管理口。
: "${EBPF_IFACE:=ens192}"
: "${EBPF_MGMT_IFACE:=ens33}"
: "${EBPF_DURATION:=10}"
: "${EBPF_RECORD_DIR:=}"

mkdir -p "${RECORD_ROOT}" "${REPORT_DIR}"

make_record_dir() {
    # 每轮测试放到独立目录，避免新旧证据混在一起。
    if [[ -n "${EBPF_RECORD_DIR}" ]]; then
        mkdir -p "${EBPF_RECORD_DIR}"
        printf '%s\n' "${EBPF_RECORD_DIR}" > "${RECORD_ROOT}/.last_record_dir"
        echo "${EBPF_RECORD_DIR}"
        return
    fi
    local d
    d="${RECORD_ROOT}/$(date +%Y%m%d-%H%M%S)-kprobe-trace-napi-poll"
    mkdir -p "${d}"
    printf '%s\n' "${d}" > "${RECORD_ROOT}/.last_record_dir"
    echo "${d}"
}

last_record_dir() {
    # 后续脚本沿用 00_check_env.sh 创建的最近一次记录目录。
    if [[ -f "${RECORD_ROOT}/.last_record_dir" ]]; then
        cat "${RECORD_ROOT}/.last_record_dir"
        return
    fi
    make_record_dir
}

run_capture() {
    local out="$1"
    shift
    echo "[cmd] $*" | tee "${out}"
    set +e
    "$@" >> "${out}" 2>&1
    local rc=$?
    set -e
    echo "RC=${rc}" >> "${out}"
    return 0
}

run_bpftrace() {
    local out="$1"
    local probe="$2"
    local duration="${3:-${EBPF_DURATION}}"
    echo "[probe] ${probe}" | tee "${out}"
    echo "[duration] ${duration}" | tee -a "${out}"
    if ! command -v bpftrace >/dev/null 2>&1; then
        echo "BPFTRACE_NOT_FOUND=1" | tee -a "${out}"
        echo "RC=127" >> "${out}"
        return 0
    fi
    set +e
    timeout "${duration}" bpftrace "${probe}" >> "${out}" 2>&1
    local rc=$?
    set -e
    echo "RC=${rc}" >> "${out}"
    # timeout returns 124 for normal time-boxed capture
    if [[ "${rc}" == "124" ]]; then
        echo "TIMEOUT_AS_EXPECTED=1" >> "${out}"
    fi
    return 0
}

kprobe_exists() {
    # 有些环境需要 sudo 才能列出 kprobe；先尝试普通用户，再尝试 sudo。
    local sym="$1"
    if bpftrace -l "kprobe:${sym}" 2>/dev/null | grep -qx "kprobe:${sym}"; then
        return 0
    fi
    if command -v sudo >/dev/null 2>&1 && sudo bpftrace -l "kprobe:${sym}" 2>/dev/null | grep -qx "kprobe:${sym}"; then
        return 0
    fi
    return 1
}

first_available_kprobe() {
    # 不同内核导出的 NAPI poll 符号不同，本 lab 只选择能被 bpftrace 看到的符号。
    command -v bpftrace >/dev/null 2>&1 || return 1
    local sym
    for sym in "$@"; do
        if kprobe_exists "${sym}"; then
            printf '%s\n' "${sym}"
            return 0
        fi
    done
    return 1
}
