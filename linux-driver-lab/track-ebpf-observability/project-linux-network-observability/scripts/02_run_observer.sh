#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${PROJ_DIR}/records"
: "${EBPF_DURATION:=15}"

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
    local out="${rd}/OBSERVER_RUN.log"
    local bin="${PROJ_DIR}/build/net_observer"

    if [[ ! -x "${bin}" ]]; then
        echo "ERROR: net_observer not built. Run scripts/01_build.sh first."
        exit 1
    fi

    echo "=== run net_observer ==="
    echo "record dir: ${rd}"
    echo "duration: ${EBPF_DURATION}s"
    echo

    {
        echo "PROJECT=project-linux-network-observability"
        echo "DATE=$(date -Iseconds)"
        echo "DURATION=${EBPF_DURATION}"
        echo
    } > "${out}"

    cd "${PROJ_DIR}"
    set +e
    if [[ "${EUID}" != "0" ]]; then
        echo "wq123456!" | sudo -S "${bin}" -v -d "${EBPF_DURATION}" >> "${out}" 2>&1
    else
        "${bin}" -v -d "${EBPF_DURATION}" >> "${out}" 2>&1
    fi
    local rc=$?
    set -e
    echo "RC=${rc}" >> "${out}"

    echo "OBSERVER_RUN=${out}"
    cat "${out}"
}

main "$@"
