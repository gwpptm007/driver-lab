#!/usr/bin/env bash
set -uo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)"
out="$($R/build/rdma-one-sided --device rxe0 --port 1 --gid-index 1)" || {
    echo "$out"
    exit 1
}
grep -q 'operation=RDMA_WRITE remote_payload=.*verify=pass' <<<"$out" || exit 1
grep -q 'operation=RDMA_READ local_payload=.*verify=pass' <<<"$out" || exit 1
grep -q 'one_sided_result=pass' <<<"$out" || exit 1
echo 'PASS: one-sided RDMA WRITE and READ'
