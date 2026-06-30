#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
APP_DIR="${LAB_ROOT}/app"
RECORD_ROOT="${LAB_ROOT}/records"
RECORD_DIR="${RECORD_ROOT}/$(date +%Y%m%d-%H%M%S)-verbs-object"

mkdir -p "${RECORD_DIR}"
ln -sfn "$(basename "${RECORD_DIR}")" "${RECORD_ROOT}/latest"

run_section() {
    local log="$1"
    local title="$2"
    shift 2

    {
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
    } 2>&1 | tee -a "${log}"
}

write_summary() {
    local summary="${RECORD_DIR}/SUMMARY.md"
    local build_log="${RECORD_DIR}/BUILD.log"
    local run_log="${RECORD_DIR}/OBJECT_LIFECYCLE.log"

    {
        printf '# RDMA Verbs Object Lifecycle Summary\n\n'
        printf '%s\n' "- Record: \`$(basename "${RECORD_DIR}")\`"
        printf '%s\n\n' "- Generated: \`$(date -Is)\`"

        printf '## Status\n\n'
        if grep -q 'BUILD_PASS' "${build_log}" 2>/dev/null; then
            printf '%s\n' '- BUILD_PASS: `rdma-object-lifecycle` compiled successfully.'
        else
            printf '%s\n' '- BUILD_FAIL: binary was not built successfully.'
        fi

        if grep -q 'OBJECT_LIFECYCLE_PASS' "${run_log}" 2>/dev/null; then
            printf '%s\n' '- OBJECT_LIFECYCLE_PASS: context/PD/MR/CQ/QP were created and destroyed.'
        elif grep -q 'NO_RDMA_DEVICES_FOUND' "${run_log}" 2>/dev/null; then
            printf '%s\n' '- BLOCKED_NO_RDMA_DEVICE: libibverbs works, but no verbs device is currently visible.'
        else
            printf '%s\n' '- OBJECT_LIFECYCLE_NOT_CONFIRMED: inspect `OBJECT_LIFECYCLE.log`.'
        fi

        printf '\n## Evidence Files\n\n'
        printf '%s\n' '- `ENV_CHECK.log`'
        printf '%s\n' '- `BUILD.log`'
        printf '%s\n' '- `OBJECT_LIFECYCLE.log`'

        printf '\n## Next Step\n\n'
        if grep -q 'OBJECT_LIFECYCLE_PASS' "${run_log}" 2>/dev/null; then
            printf 'Use this result as the base for QP state transition: `RESET -> INIT -> RTR -> RTS`.\n'
        else
            printf 'Enable a verbs device with hardware RDMA or Soft-RoCE, then rerun this lab.\n'
        fi
    } | tee "${summary}"
}

printf '== RDMA verbs object lifecycle lab ==\n'
printf 'LAB_ROOT=%s\n' "${LAB_ROOT}"
printf 'RECORD_DIR=%s\n\n' "${RECORD_DIR}"

env_log="${RECORD_DIR}/ENV_CHECK.log"
build_log="${RECORD_DIR}/BUILD.log"
run_log="${RECORD_DIR}/OBJECT_LIFECYCLE.log"

{
    printf '# Environment check\n'
    printf 'timestamp=%s\n' "$(date -Is)"
} > "${env_log}"
run_section "${env_log}" "uname" uname -a
run_section "${env_log}" "compiler" sh -c 'command -v gcc || command -v cc || true'
run_section "${env_log}" "make" sh -c 'command -v make || true'
run_section "${env_log}" "pkg-config libibverbs" sh -c 'pkg-config --cflags --libs libibverbs 2>/dev/null || true'
run_section "${env_log}" "verbs packages" sh -c "dpkg -l | grep -E 'libibverbs|ibverbs-providers|ibverbs-utils|rdma-core' || true"
run_section "${env_log}" "verbs header" sh -c 'test -f /usr/include/infiniband/verbs.h && echo VERBS_HEADER_PRESENT || echo VERBS_HEADER_MISSING'
run_section "${env_log}" "rdma dev" sh -c 'rdma dev 2>/dev/null || true'

{
    printf '# Build\n'
    printf 'timestamp=%s\n' "$(date -Is)"
} > "${build_log}"
(
    cd "${APP_DIR}"
    run_section "${build_log}" "make clean" make clean
    run_section "${build_log}" "make" make
)
if [[ -x "${APP_DIR}/rdma-object-lifecycle" ]]; then
    printf 'BUILD_PASS binary=%s\n' "${APP_DIR}/rdma-object-lifecycle" | tee -a "${build_log}"
else
    printf 'BUILD_FAIL binary_missing\n' | tee -a "${build_log}"
fi

{
    printf '# Object lifecycle run\n'
    printf 'timestamp=%s\n' "$(date -Is)"
} > "${run_log}"
run_section "${run_log}" "rdma resource before" sh -c 'rdma resource show 2>/dev/null || true'
run_section "${run_log}" "rdma-object-lifecycle" "${APP_DIR}/rdma-object-lifecycle"
run_section "${run_log}" "rdma resource after" sh -c 'rdma resource show 2>/dev/null || true'

write_summary

printf '\n== Latest summary ==\n'
cat "${RECORD_DIR}/SUMMARY.md"
