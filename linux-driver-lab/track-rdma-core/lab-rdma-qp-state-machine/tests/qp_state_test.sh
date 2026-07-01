#!/usr/bin/env bash
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT}/build/rdma-qp-state"

[[ -x "${BIN}" ]] || { echo "FAIL: binary missing"; exit 1; }
"${BIN}" --help | grep -q 'usage: rdma-qp-state' || exit 1

device="$(${BIN} --list | sed -n 's/^device\[[0-9]*\]=//p' | head -n 1)"
if [[ -z "${device}" ]]; then
    echo 'SKIP: no RDMA device'
    exit 0
fi

output="$(${BIN} --device "${device}" --port 1 --gid-index 1)" || {
    printf '%s\n' "${output}"
    exit 1
}
grep -q 'negative=RESET_to_RTR.*result=pass' <<<"${output}" || exit 1
grep -q 'endpoint=left.*state=RTS' <<<"${output}" || exit 1
grep -q 'endpoint=right.*state=RTS' <<<"${output}" || exit 1
grep -q 'state_machine_result=pass' <<<"${output}" || exit 1
grep -q 'cleanup=complete result=pass' <<<"${output}" || exit 1
echo 'PASS: help, invalid transition, two endpoints RTS, cleanup'
