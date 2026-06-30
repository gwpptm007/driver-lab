#!/usr/bin/env bash
set -euo pipefail

LAB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RECORD_ROOT="${LAB_ROOT}/records"

latest_record_dir() {
    find "${RECORD_ROOT}" -maxdepth 1 -type d -name '20*-rdma-env' | sort | tail -n 1
}

ensure_record_dir() {
    mkdir -p "${RECORD_ROOT}"
    local latest
    latest="$(latest_record_dir || true)"
    if [[ -n "${latest}" && "${REUSE_LATEST_RECORD:-1}" == "1" ]]; then
        printf '%s\n' "${latest}"
        return 0
    fi
    local ts
    ts="$(date +%Y%m%d-%H%M%S)"
    local dir="${RECORD_ROOT}/${ts}-rdma-env"
    mkdir -p "${dir}"
    printf '%s\n' "${dir}"
}

run_or_note() {
    local title="$1"
    shift
    printf '\n===== %s =====\n' "${title}"
    printf '$'
    printf ' %q' "$@"
    printf '\n'
    if "$@"; then
        return 0
    fi
    local rc=$?
    printf '[command_exit_code=%s]\n' "${rc}"
    return 0
}

command_status() {
    local cmd="$1"
    if command -v "${cmd}" >/dev/null 2>&1; then
        printf 'CMD_PRESENT %s %s\n' "${cmd}" "$(command -v "${cmd}")"
    else
        printf 'CMD_MISSING %s\n' "${cmd}"
    fi
}
