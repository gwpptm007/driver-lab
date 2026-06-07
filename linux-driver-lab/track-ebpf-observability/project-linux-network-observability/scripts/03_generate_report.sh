#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${PROJ_DIR}/records"
REPORT_ROOT="${PROJ_DIR}/reports"
: "${EBPF_DURATION:=15}"
: "${EBPF_IFACE:=""}"

last_record_dir() {
    if [[ -f "${RECORD_ROOT}/.last_record_dir" ]]; then
        cat "${RECORD_ROOT}/.last_record_dir"
    else
        local rd="${RECORD_ROOT}/run-$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$rd"
        echo "$rd"
    fi
}

main() {
    local rd
    rd="$(last_record_dir)"
    local report_ts="$(date +%Y%m%d-%H%M%S)"
    local report_file="${REPORT_ROOT}/net-observe-${report_ts}.md"
    local run_log="${rd}/OBSERVER_RUN.log"

    mkdir -p "${REPORT_ROOT}"

    local bin="${PROJ_DIR}/build/net_observer"

    if [[ ! -x "${bin}" ]]; then
        echo "ERROR: net_observer not built. Run scripts/01_build.sh first."
        exit 1
    fi

    echo "=== run net_observer + generate report ==="
    echo "record dir: ${rd}"
    echo "report: ${report_file}"
    echo "duration: ${EBPF_DURATION}s"
    echo

    {
        echo "PROJECT=project-linux-network-observability"
        echo "DATE=$(date -Iseconds)"
        echo "DURATION=${EBPF_DURATION}"
        echo "REPORT=${report_file}"
        echo
    } > "${run_log}"

    cd "${PROJ_DIR}"

    # 构建参数
    local args="-v -d ${EBPF_DURATION} -o ${report_file}"
    if [[ -n "${EBPF_IFACE}" ]]; then
        args="${args} -i ${EBPF_IFACE}"
    fi

    set +e
    if [[ "${EUID}" != "0" ]]; then
        echo "wq123456!" | sudo -S "${bin}" ${args} >> "${run_log}" 2>&1
    else
        "${bin}" ${args} >> "${run_log}" 2>&1
    fi
    local rc=$?
    set -e
    echo "RC=${rc}" >> "${run_log}"

    echo "OBSERVER_RUN=${run_log}"
    if [[ -f "${report_file}" ]]; then
        echo "REPORT=${report_file}"
        echo
        echo "=== Report Preview ==="
        cat "${report_file}"
    fi
}

main "$@"
