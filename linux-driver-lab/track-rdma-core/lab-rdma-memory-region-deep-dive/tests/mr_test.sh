#!/usr/bin/env bash
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT}/build/rdma-mr-lab"
pass=0
fail=0

check() {
    local name="$1" expected="$2"
    shift 2
    local output
    if output="$("$@" 2>&1)" && grep -Fq "${expected}" <<<"${output}"; then
        printf 'PASS: %s\n' "${name}"
        pass=$((pass + 1))
    else
        printf 'FAIL: %s\n%s\n' "${name}" "${output}" >&2
        fail=$((fail + 1))
    fi
}

[[ -x "${BIN}" ]] || { echo "FAIL: binary missing: ${BIN}" >&2; exit 1; }

check "help" "usage: rdma-mr-lab" "${BIN}" --help
check "list" "device_count=" "${BIN}" --list

device="$(${BIN} --list 2>/dev/null | sed -n 's/^device\[[0-9][0-9]*\]=//p' | head -n 1)"
if [[ -z "${device}" ]]; then
    printf 'SKIP: MR suite: no provider-visible device\n'
else
    check "MR experiment suite" "suite_result=pass" "${BIN}" --device "${device}" --port 1
fi

printf 'SUMMARY: pass=%d fail=%d\n' "${pass}" "${fail}"
[[ ${fail} -eq 0 ]]
