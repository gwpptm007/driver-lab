#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${PROJ_DIR}/records"

last_record_dir() {
    if [[ -f "${RECORD_ROOT}/.last_record_dir" ]]; then
        cat "${RECORD_ROOT}/.last_record_dir"
    else
        local rd="${RECORD_ROOT}/build-$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$rd"
        echo "$rd"
    fi
}

main() {
    local rd
    rd="$(last_record_dir)"
    local out="${rd}/BUILD.log"

    echo "=== build project-linux-network-observability ==="
    echo "record dir: ${rd}"
    echo

    cd "${PROJ_DIR}"

    # Step 0: check tools
    echo "[check] tools..."
    for t in clang bpftool gcc make; do
        if ! command -v "$t" >/dev/null 2>&1; then
            echo "ERROR: $t not found, install: sudo apt install clang bpftool libbpf-dev make"
            exit 1
        fi
    done
    echo "[check] OK"

    # Build
    echo "[build] make clean && make"
    make clean 2>/dev/null || true
    make 2>&1 | tee "${out}"

    if [[ -x "${PROJ_DIR}/build/net_observer" ]]; then
        echo
        echo "BUILD_OK=1" | tee -a "${out}"
        echo "Binary: ${PROJ_DIR}/build/net_observer"
        echo "${rd}" > "${RECORD_ROOT}/.last_record_dir"
    else
        echo
        echo "BUILD_FAIL=1" | tee -a "${out}"
        echo "ERROR: build failed, see ${out}"
        return 1
    fi
}

main "$@"
