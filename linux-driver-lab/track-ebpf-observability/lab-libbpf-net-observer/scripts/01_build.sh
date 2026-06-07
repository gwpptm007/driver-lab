#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORD_ROOT="${LAB_DIR}/records"

last_record_dir() {
    if [[ -f "${RECORD_ROOT}/.last_record_dir" ]]; then
        cat "${RECORD_ROOT}/.last_record_dir"
    else
        echo "${RECORD_ROOT}/build-$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$_"
    fi
}

main() {
    local rd
    rd="$(last_record_dir)"
    local out="${rd}/BUILD.log"

    echo "=== build lab-libbpf-net-observer ==="
    echo "record dir: ${rd}"
    echo

    cd "${LAB_DIR}"

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

    if [[ -x "${LAB_DIR}/build/skb_observer" ]]; then
        echo
        echo "BUILD_OK=1" | tee -a "${out}"
        echo "Binary: ${LAB_DIR}/build/skb_observer"
    else
        echo
        echo "BUILD_FAIL=1" | tee -a "${out}"
        echo "ERROR: build failed, see ${out}"
        return 1
    fi
}

main "$@"
