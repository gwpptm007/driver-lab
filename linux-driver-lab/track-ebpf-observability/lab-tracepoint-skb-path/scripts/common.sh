#!/usr/bin/env bash
set -euo pipefail

LAB_NAME="lab-tracepoint-skb-path"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${LAB_DIR}/records"
REPORT_DIR="${LAB_DIR}/reports"

: "${EBPF_IFACE:=ens192}"
: "${EBPF_MGMT_IFACE:=ens33}"
: "${EBPF_DURATION:=10}"
: "${EBPF_RECORD_DIR:=}"

mkdir -p "${RECORD_ROOT}" "${REPORT_DIR}"

make_record_dir() {
    if [[ -n "${EBPF_RECORD_DIR}" ]]; then
        mkdir -p "${EBPF_RECORD_DIR}"
        printf '%s\n' "${EBPF_RECORD_DIR}" > "${RECORD_ROOT}/.last_record_dir"
        echo "${EBPF_RECORD_DIR}"
        return
    fi
    local d
    d="${RECORD_ROOT}/$(date +%Y%m%d-%H%M%S)-tracepoint-skb-path"
    mkdir -p "${d}"
    printf '%s\n' "${d}" > "${RECORD_ROOT}/.last_record_dir"
    echo "${d}"
}

last_record_dir() {
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

tracepoint_exists() {
    local tp="$1"
    if bpftrace -l "tracepoint:${tp}" 2>/dev/null | grep -qx "tracepoint:${tp}"; then
        return 0
    fi
    if command -v sudo >/dev/null 2>&1 && sudo bpftrace -l "tracepoint:${tp}" 2>/dev/null | grep -qx "tracepoint:${tp}"; then
        return 0
    fi
    return 1
}

first_available_tracepoint() {
    local tp
    for tp in "$@"; do
        if tracepoint_exists "${tp}"; then
            printf '%s\n' "${tp}"
            return 0
        fi
    done
    return 1
}
