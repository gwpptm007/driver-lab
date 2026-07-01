#!/usr/bin/env bash
set -uo pipefail
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/.."&&pwd)"
out="$($R/build/rdma-rc-pingpong --device rxe0 --port 1 --gid-index 1)"||{ echo "$out";exit 1; }
grep -q 'round=ping.*verify=pass'<<<"$out"||exit 1
grep -q 'round=pong.*verify=pass'<<<"$out"||exit 1
grep -q 'pingpong_result=pass'<<<"$out"||exit 1
echo 'PASS: RC ping and pong with successful CQEs'
