#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# pp_check.sh — 验证 page_pool 分配和回收
#
# 通过标准：
#   1. pp_alloc 增量 > 0（证明 page_pool 真正分配了 page）
#   2. pp_build_skb_fail = 0（build_skb 没有失败）
#
# 说明：
#   pp_recycle 正常情况下为 0（成功路径靠 skb destructor 隐式回收，
#   不经过 page_pool_recycle_direct）
#   pp_build_skb_fail > 0 且 pp_recycle > 0 时说明 build_skb 失败了，
#   但那是异常路径，不是主要验收标准。

set -euo pipefail
DIR=${1:-}
[[ -n "$DIR" ]] || { echo "usage: $0 <record-dir>" >&2; exit 1; }
PP_BEFORE="$DIR/debugfs_page_pool_before.txt"
PP_AFTER="$DIR/debugfs_page_pool_after.txt"
[[ -f "$PP_AFTER" ]] || { echo "missing $PP_AFTER" >&2; exit 1; }
[[ -f "$PP_BEFORE" ]] || { echo "missing $PP_BEFORE" >&2; exit 1; }

# 提取 qN pp_alloc=N 格式
before_alloc=$(grep -oE 'q[0-9]+: .* pp_alloc=[0-9]+' "$PP_BEFORE" | sed 's/q[0-9]*: .* pp_alloc=//' | tr '\n' ' ' | tr -d ' ')
after_alloc=$(grep -oE 'q[0-9]+: .* pp_alloc=[0-9]+' "$PP_AFTER" | sed 's/q[0-9]*: .* pp_alloc=//' | tr '\n' ' ' | tr -d ' ')

before_arr=($before_alloc)
after_arr=($after_alloc)

if [[ ${#before_arr[@]} -ne ${#after_arr[@]} ]]; then
    echo "pp_check FAILED: queue count mismatch"
    exit 1
fi

# 计算每个队列的 pp_alloc 增量
total_delta=0
fail_count=0
for i in "${!before_arr[@]}"; do
    delta=$((${after_arr[$i]} - ${before_arr[$i]}))
    total_delta=$((total_delta + delta))
done

# pp_build_skb_fail 检查
before_fail=$(grep -oE 'q[0-9]+: .* pp_build_skb_fail=[0-9]+' "$PP_BEFORE" | sed 's/q[0-9]*: .* pp_build_skb_fail=//' | tr '\n' ' ' | tr -d ' ')
after_fail=$(grep -oE 'q[0-9]+: .* pp_build_skb_fail=[0-9]+' "$PP_AFTER" | sed 's/q[0-9]*: .* pp_build_skb_fail=//' | tr '\n' ' ' | tr -d ' ')
fail_delta=0
if [[ -n "$before_fail" && -n "$after_fail" ]]; then
    before_fail_arr=($before_fail)
    after_fail_arr=($after_fail)
    for i in "${!before_fail_arr[@]}"; do
        delta=$((${after_fail_arr[$i]} - ${before_fail_arr[$i]}))
        fail_delta=$((fail_delta + delta))
    done
fi

if [[ $total_delta -gt 0 ]] && [[ $fail_delta -eq 0 ]]; then
    echo "pp_check PASSED: total pp_alloc delta=$total_delta, pp_build_skb_fail delta=$fail_delta"
    echo "  pp_alloc deltas: before='$before_alloc' after='$after_alloc'"
    exit 0
fi

echo "pp_check FAILED: total pp_alloc delta=$total_delta (need > 0), pp_build_skb_fail delta=$fail_delta (need = 0)"
echo "  pp_alloc before: $before_alloc"
echo "  pp_alloc after:  $after_alloc"
exit 1