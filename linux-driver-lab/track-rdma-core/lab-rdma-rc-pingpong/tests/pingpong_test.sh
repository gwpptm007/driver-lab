#!/usr/bin/env bash
set -uo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)"
# GID index 由当前端口表决定，默认 0，也可由回归环境显式覆盖。
gid_index="${RDMA_GID_INDEX:-0}"
out="$($R/build/rdma-rc-pingpong --device rxe0 --port 1 --gid-index "${gid_index}")"||{ echo "$out";exit 1; }
grep -q 'round=ping.*verify=pass'<<<"$out"||exit 1
grep -q 'round=pong.*verify=pass'<<<"$out"||exit 1
grep -q 'pingpong_result=pass'<<<"$out"||exit 1
echo 'PASS: RC ping and pong with successful CQEs'
