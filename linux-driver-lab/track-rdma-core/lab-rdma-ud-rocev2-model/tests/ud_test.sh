#!/usr/bin/env bash
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="$(${ROOT}/build/rdma-ud-demo --device rxe0 --port 1 --gid-index 1)" || {
    echo "${output}"
    exit 1
}
grep -q 'transport=UD' <<<"${output}" || exit 1
grep -q 'grh_bytes=40.*verify=pass' <<<"${output}" || exit 1
grep -q 'ud_result=pass' <<<"${output}" || exit 1
echo 'PASS: UD datagram, GRH offset and payload'
