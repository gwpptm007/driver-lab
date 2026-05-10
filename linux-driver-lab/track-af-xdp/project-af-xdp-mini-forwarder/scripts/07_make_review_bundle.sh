#!/usr/bin/env bash
#============================================================
# 07_make_review_bundle.sh — 生成测试结论包
#
# 功能：
#   分析本轮所有日志文件，综合判断：
#   PASS_BUILD / PASS_DROP_SMOKE / PASS_REFLECT_SMOKE 等，
#   并输出 REVIEW_BUNDLE.md。
#
# 判定逻辑：
#   PASS_BUILD              — BUILD.log 中 BUILD_RESULT=PASS
#   PASS_DROP_SMOKE         — FORWARDER_DROP.log 中 AF_XDP_FORWARDER_READY 出现
#   PASS_DROP_FINAL         — FORWARDER_DROP.log 中 FORWARDER_FINAL_STATS 出现
#   PASS_REFLECT_SMOKE     — FORWARDER_REFLECT.log 中 AF_XDP_FORWARDER_READY 出现
#   PASS_REFLECT_FINAL     — FORWARDER_REFLECT.log 中 FORWARDER_FINAL_STATS 出现
#   PASS_TRAFFIC           — rx_packets > 0（需要外部发包）
#   PASS_TX_REFLECT        — tx_packets > 0（反映 TX 路径）
#
# 使用：
#   ./scripts/07_make_review_bundle.sh
#   或
#   ./scripts/07_make_review_bundle.sh /path/to/RECORD_DIR
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(latest_record_dir)"
out="${record_dir}/REVIEW_BUNDLE.md"

# 检查文件是否存在且非空
check_file() { local f="$1"; if [[ -s "${record_dir}/${f}" ]]; then echo "DONE"; else echo "MISSING"; fi; }

# 检查文件中是否包含指定文本
has_text() { local f="$1"; shift; local p="$*"; grep -q "${p}" "${record_dir}/${f}" 2>/dev/null && echo "YES" || echo "NO"; }

{
    echo "# AF_XDP mini forwarder review bundle"
    echo
    echo "## Record"
    echo "\`${record_dir}\`"
    echo
    echo "## Files"
    echo "| File | Status |"
    echo "|---|---|"
    for f in ENV_CHECK.txt BUILD.log PREPARE_KERNEL_NETDEV.txt FORWARDER_DROP.log FORWARDER_REFLECT.log COLLECT_STATS.txt TRAFFIC_HINT.txt; do
        echo "| ${f} | $(check_file "$f") |"
    done
    echo
    echo "## Acceptance"
    echo "| Item | Result |"
    echo "|---|---|"
    echo "| PASS_BUILD | $(has_text BUILD.log BUILD_RESULT=PASS) |"
    echo "| PASS_DROP_SMOKE | $(has_text FORWARDER_DROP.log AF_XDP_FORWARDER_READY) |"
    echo "| PASS_DROP_FINAL | $(has_text FORWARDER_DROP.log FORWARDER_FINAL_STATS) |"
    echo "| PASS_REFLECT_SMOKE | $(has_text FORWARDER_REFLECT.log AF_XDP_FORWARDER_READY) |"
    echo "| PASS_REFLECT_FINAL | $(has_text FORWARDER_REFLECT.log FORWARDER_FINAL_STATS) |"
    echo
    echo "## Parsed stats"
    echo '```text'
    if [[ -f "${record_dir}/COLLECT_STATS.txt" ]]; then
        sed -n '/== parsed forwarder stats ==/,$p' "${record_dir}/COLLECT_STATS.txt" || true
    fi
    echo '```'
    echo
    echo "## Notes"
    echo "- 无流量时 rx/tx 为 0 是正常的 smoke 结果，不代表失败。"
    echo "- PASS_TRAFFIC 需要 rx_packets > 0，需要外部发包。"
    echo "- PASS_TX_REFLECT 需要 tx_packets > 0 和 comp_packets > 0。"
} > "${out}"
echo "WROTE ${out}"