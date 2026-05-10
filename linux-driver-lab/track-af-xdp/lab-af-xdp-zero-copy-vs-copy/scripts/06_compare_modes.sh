#!/usr/bin/env bash
#============================================================
# 06_compare_modes.sh — 对比 4 种模式探测结果
#
# 功能：
#   调用 tools/parse_mode_results.py 解析本轮所有 probe 日志，
#   生成 COMPARE_MODES.txt，包含各模式的成功/失败对比。
#
# 使用：
#   ./scripts/06_compare_modes.sh
#   或
#   ./scripts/06_compare_modes.sh /path/to/RECORD_DIR
#
# 输出：
#   COMPARE_MODES.txt（包含 4 种模式的返回值、关键日志标记对比）
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="${1:-$(latest_record_dir)}"
out="${record_dir}/COMPARE_MODES.txt"

{
    write_env_header
    echo

    # 解析所有 probe 日志，输出对比表格
    python3 "${LAB_DIR}/tools/parse_mode_results.py" "${record_dir}"
    echo

    echo "== Interpretation =="
    echo "- skb/copy 是兼容性基线，最容易成功。"
    echo "- native/copy 通过说明驱动支持 native XDP attach。"
    echo "- native/zero-copy 成功才说明真正支持 AF_XDP ZC。"
    echo "- vmxnet3 上 zero-copy 失败是可接受的，只要记录清楚 fallback 策略。"
} | tee "${out}"