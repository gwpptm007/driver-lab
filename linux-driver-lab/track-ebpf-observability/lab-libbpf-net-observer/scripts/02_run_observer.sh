#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${LAB_DIR}/records"
: "${EBPF_DURATION:=10}"

last_record_dir() {
    if [[ -f "${RECORD_ROOT}/.last_record_dir" ]]; then
        cat "${RECORD_ROOT}/.last_record_dir"
    else
        echo "${RECORD_ROOT}/run-$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$_"
    fi
}

main() {
    local rd
    rd="$(last_record_dir)"
    local out="${rd}/OBSERVER_RUN.log"
    local bin="${LAB_DIR}/build/skb_observer"

    if [[ ! -x "${bin}" ]]; then
        echo "ERROR: skb_observer not built. Run scripts/01_build.sh first."
        exit 1
    fi

    echo "=== run skb_observer ==="
    echo "record dir: ${rd}"
    echo "duration: ${EBPF_DURATION}s"
    echo

    {
        echo "LAB=lab-libbpf-net-observer"
        echo "DATE=$(date -Iseconds)"
        echo "DURATION=${EBPF_DURATION}"
        echo
    } > "${out}"

    set +e
    cd "${LAB_DIR}"
    if [[ "${EUID}" != "0" ]]; then
        # 凭据只从调用环境传入；未提供时由 sudo 正常交互，禁止写死在仓库中。
        if [[ -n "${SUDO_PASSWORD:-}" ]]; then
            printf '%s\n' "${SUDO_PASSWORD}" | sudo -S "${bin}" -v -d "${EBPF_DURATION}" >> "${out}" 2>&1
        else
            sudo "${bin}" -v -d "${EBPF_DURATION}" >> "${out}" 2>&1
        fi
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
