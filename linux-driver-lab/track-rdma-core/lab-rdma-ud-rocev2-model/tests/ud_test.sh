#!/usr/bin/env bash
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# RXE 重建后 GID 表可能重排，默认使用必需的 index 0，并保留环境覆盖入口。
gid_index="${RDMA_GID_INDEX:-0}"
output="$(${ROOT}/build/rdma-ud-demo --device rxe0 --port 1 --gid-index "${gid_index}")" || {
    echo "${output}"
    exit 1
}
grep -q 'transport=UD' <<<"${output}" || exit 1
grep -q 'grh_bytes=40.*verify=pass' <<<"${output}" || exit 1
grep -q 'ud_result=pass' <<<"${output}" || exit 1
echo 'PASS: UD datagram, GRH offset and payload'
