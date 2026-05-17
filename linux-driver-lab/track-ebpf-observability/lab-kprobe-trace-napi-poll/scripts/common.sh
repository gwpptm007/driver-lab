#!/usr/bin/env bash
set -euo pipefail

LAB_NAME="lab-kprobe-trace-napi-poll"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${LAB_DIR}/records"
REPORT_DIR="${LAB_DIR}/reports"
PROBE_DIR="${LAB_DIR}/probes"

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
    d="${RECORD_ROOT}/$(date +%Y%m%d-%H%M%S)-kprobe-trace-napi-poll"
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
