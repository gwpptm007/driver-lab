#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# vector_check.sh — 验证 MSI-X vector 处理（pci 版本）
#
# 通过标准：至少有 2 个 vector 的 handle_count >= 1
# pci 版本的 vector 通过 /proc/interrupts 观测，
# 但 debugfs vectors 也记录了 handle 次数。

set -euo pipefail
DIR=${1:-}
[[ -n "$DIR" ]] || { echo "usage: $0 <record-dir>" >&2; exit 1; }

# pci 版本 debugfs 路径
VEC="$DIR/debugfs_vectors_after.txt"
[[ -f "$VEC" ]] || { echo "missing $VEC" >&2; exit 1; }

# 统计有 handle 处理的 vector 数量
handles=$(grep -cE '^vector[0-9]+: .* handle=[1-9][0-9]*' "$VEC" 2>/dev/null || echo 0)
if [[ "$handles" -ge 2 ]]; then
    echo "vector check PASSED: $handles vectors with handle_count >= 1"
    exit 0
fi

echo "vector check FAILED: only $handles vectors with handle_count >= 1 (need >= 2)"
exit 1
