#!/usr/bin/env bash
set -uo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)"
# 不依赖历史临时地址生成的固定 GID index，测试机可通过环境变量覆盖。
gid_index="${RDMA_GID_INDEX:-0}"
out="$($R/build/rdma-one-sided --device rxe0 --port 1 --gid-index "${gid_index}")" || {
    echo "$out"
    exit 1
}
grep -q 'operation=RDMA_WRITE remote_payload=.*verify=pass' <<<"$out" || exit 1
grep -q 'operation=RDMA_READ local_payload=.*verify=pass' <<<"$out" || exit 1
grep -q 'one_sided_result=pass' <<<"$out" || exit 1
echo 'PASS: one-sided RDMA WRITE and READ'
