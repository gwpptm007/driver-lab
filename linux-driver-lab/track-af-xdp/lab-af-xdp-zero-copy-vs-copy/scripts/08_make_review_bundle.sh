#!/usr/bin/env bash
#============================================================
# 08_make_review_bundle.sh — 生成测试结论包
#
# 功能：
#   分析本轮所有探测结果，生成 REVIEW_BUNDLE.md，
 *   包含环境信息、文件存在性检查、4种模式对比结果、最终判定。
 *
 * 判定规则：
 *   - PASS_COPY_BASELINE=YES：skb+copy 基线测试通过（XSK_SOCKET_READY）
 *   - ZERO_COPY_PROBED=YES：zero-copy 探测已执行（无论成功失败）
 *   - PASS_ZERO_COPY=YES：native+zero-copy 成功（XSK socket 创建成功）
 *   - vmxnet3 上零拷贝失败是可接受的，只要记录清楚 fallback
 *
 * 使用：
 *   ./scripts/08_make_review_bundle.sh
 *   或
 *   ./scripts/08_make_review_bundle.sh /path/to/RECORD_DIR
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="${1:-$(latest_record_dir)}"
out="${record_dir}/REVIEW_BUNDLE.md"
compare="${record_dir}/COMPARE_MODES.txt"

# 若 COMPARE_MODES.txt 尚不存在，先调用 06 脚本生成
if [[ ! -f "${compare}" ]]; then
    "${SCRIPT_DIR}/06_compare_modes.sh" "${record_dir}" >/dev/null || true
fi

{
    echo "# lab-af-xdp-zero-copy-vs-copy REVIEW_BUNDLE"
    echo
    echo "## Environment"
    echo
    echo '```text'
    write_env_header
    echo '```'
    echo

    # 文件存在性检查
    echo "## Evidence files"
    echo
    echo "| File | Status |"
    echo "|---|---|"
    for f in ENV_CHECK.txt BUILD.log PREPARE_KERNEL_NETDEV.txt COPY_BASELINE.log NATIVE_COPY_PROBE.log ZERO_COPY_PROBE.log COMPARE_MODES.txt COLLECT_STATS.txt; do
        if [[ -f "${record_dir}/${f}" ]]; then
            echo "| ${f} | DONE |"
        else
            echo "| ${f} | MISSING |"
        fi
    done
    echo

    # 模式对比结果
    echo "## Mode comparison"
    echo
    echo '```text'
    if [[ -f "${compare}" ]]; then
        cat "${compare}"
    else
        echo "COMPARE_MODES.txt missing"
    fi
    echo '```'
    echo

    # 最终判定
    echo "## Verdict guide"
    echo
    echo "- \`PASS_COPY_BASELINE=YES\`：skb+copy 基线测试通过（XSK_SOCKET_READY 出现）。"
    echo "- \`ZERO_COPY_PROBED=YES\`：zero-copy 探测已实际执行。"
    echo "- \`PASS_ZERO_COPY=YES\`：native+zero-copy 成功，XSK socket 创建成功。"
    echo "- vmxnet3 上零拷贝失败是可接受的，只要 fallback 策略已记录。"
} > "${out}"

echo "REVIEW_BUNDLE=${out}"