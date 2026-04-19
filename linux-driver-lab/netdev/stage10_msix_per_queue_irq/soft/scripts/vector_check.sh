#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# vector_check.sh — 验证 MSI-X vector 处理
#
# 通过标准：至少有 2 个 vector 的 handle_count 增量 >= 1
# 这证明 vector irq_work 被真正调度和执行了。
#
# 修复记录（2026-04-19）：
#   - 旧版只检查 after，导致历史 handle_count > 0 就 PASS（假通过）
#   - 新版检查 before/after 增量，确保本轮测试真正触发了向量中断

set -euo pipefail
DIR=${1:-}
[[ -n "$DIR" ]] || { echo "usage: $0 <record-dir>" >&2; exit 1; }
VEC_BEFORE="$DIR/debugfs_vectors_before.txt"
VEC_AFTER="$DIR/debugfs_vectors_after.txt"
[[ -f "$VEC_AFTER" ]] || { echo "missing $VEC_AFTER" >&2; exit 1; }
[[ -f "$VEC_BEFORE" ]] || { echo "missing $VEC_BEFORE" >&2; exit 1; }

# 提取 vectorN: handle=N 格式，提取编号和 handle 值
# 格式：vector0: raise=10 handle=5 schedule=10
before_handles=$(grep -oE '^vector[0-9]+: .* handle=[0-9]+' "$VEC_BEFORE" | sed 's/^vector\([0-9]\+\): .* handle=//' | sort -n)
after_handles=$(grep -oE '^vector[0-9]+: .* handle=[0-9]+' "$VEC_AFTER" | sed 's/^vector\([0-9]\+\): .* handle=//' | sort -n)

before_arr=($before_handles)
after_arr=($after_handles)

if [[ ${#before_arr[@]} -ne ${#after_arr[@]} ]]; then
    echo "vector check FAILED: vector count mismatch before=${#before_arr[@]} after=${#after_arr[@]}"
    exit 1
fi

# 计算每个向量的增量
pass_count=0
total=0
deltas=""
for i in "${!before_arr[@]}"; do
    delta=$((${after_arr[$i]} - ${before_arr[$i]}))
    deltas="$deltas $delta"
    if [[ $delta -ge 1 ]]; then
        pass_count=$((pass_count + 1))
    fi
    total=$((total + 1))
done

if [[ "$pass_count" -ge 2 ]] && [[ "$total" -ge 2 ]]; then
    echo "vector check PASSED: $pass_count/$total vectors with handle_count delta >= 1"
    echo "  handle deltas:$deltas"
    exit 0
fi

echo "vector check FAILED: only $pass_count/$total vectors with handle_count delta >= 1 (need >= 2)"
echo "  handle deltas:$deltas"
exit 1