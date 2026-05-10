#!/usr/bin/env bash
#============================================================
# 06_make_review_bundle.sh — 生成测试结论包
#
# 功能：
#   分析本轮测试的所有日志文件，综合判断：
#   PASS_SOCKET_READY / PASS_UMEM_RINGS / PASS_RX_TRAFFIC
#   并输出 REVIEW_BUNDLE.md（供人工复核和问题记录）。
#
# 判断逻辑：
#   PASS_SOCKET_READY : XSK_SOCKET_READY + XSKMAP_REGISTERED 均出现
#   PASS_UMEM_RINGS   : UMEM_READY + FILL_RING_READY + AF_XDP_RINGS_READY 均出现
#   PASS_RX_TRAFFIC   : AF_XDP_FINAL_STATS 中 rx_packets > 0
#
# 使用：
#   ./scripts/06_make_review_bundle.sh
#   或
#   ./scripts/06_make_review_bundle.sh /path/to/RECORD_DIR
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 支持指定记录目录（默认使用最新目录）
REC_DIR="${1:-$(latest_record_dir)}"
OUT="${REC_DIR}/REVIEW_BUNDLE.md"
SMOKE_LOG="${REC_DIR}/AF_XDP_SOCKET_SMOKE.log"

#------------------------------
# have_marker — 检查日志中是否存在指定标记
#------------------------------
have_marker() {
    local marker="$1"
    [[ -f "${SMOKE_LOG}" ]] && grep -q "${marker}" "${SMOKE_LOG}"
}

#------------------------------
# 解析 rx_packets 数值
#------------------------------
rx_pkts="0"
if [[ -f "${SMOKE_LOG}" ]]; then
    rx_pkts="$(grep 'AF_XDP_FINAL_STATS' "${SMOKE_LOG}" | tail -1 | sed -n 's/.*rx_packets=\([0-9][0-9]*\).*/\1/p')"
    rx_pkts="${rx_pkts:-0}"
fi

#------------------------------
# 综合判断
#------------------------------
pass_socket="NO"
pass_rings="NO"
pass_rx="NO"
have_marker "XSK_SOCKET_READY" && have_marker "XSKMAP_REGISTERED" && pass_socket="YES"
have_marker "UMEM_READY" && have_marker "FILL_RING_READY" && have_marker "AF_XDP_RINGS_READY" && pass_rings="YES"
if [[ "${rx_pkts}" =~ ^[0-9]+$ ]] && (( rx_pkts > 0 )); then
    pass_rx="YES"
fi

{
    echo "# REVIEW_BUNDLE: ${LAB_NAME}"
    echo
    echo "## Environment"
    echo
    echo '```text'
    write_env_header
    echo '```'
    echo
    echo "## Files"
    echo
    for f in ENV_CHECK.txt BUILD.log PREPARE_KERNEL_NETDEV.txt AF_XDP_SOCKET_SMOKE_COMMAND.txt AF_XDP_SOCKET_SMOKE.log TRAFFIC_HINT.txt COLLECT_STATS.txt; do
        if [[ -f "${REC_DIR}/${f}" ]]; then
            echo "- ${f}: DONE"
        else
            echo "- ${f}: MISSING"
        fi
    done
    echo
    echo "## Verdict"
    echo
    echo "| Item | Result |"
    echo "|---|---|"
    echo "| PASS_SOCKET_READY | ${pass_socket} |"
    echo "| PASS_UMEM_RINGS | ${pass_rings} |"
    echo "| PASS_RX_TRAFFIC | ${pass_rx} |"
    echo "| rx_packets | ${rx_pkts} |"
    echo
    echo "## Interpretation"
    echo
    if [[ "${pass_socket}" == "YES" && "${pass_rings}" == "YES" ]]; then
        echo "The AF_XDP socket, UMEM, FILL/RX/TX/COMPLETION ring setup path is ready."
    else
        echo "The AF_XDP socket/ring setup path is incomplete; check AF_XDP_SOCKET_SMOKE.log."
    fi
    if [[ "${pass_rx}" == "YES" ]]; then
        echo "The socket received traffic through XDP redirect, so this lab reaches PASS_RX_TRAFFIC."
    else
        echo "No RX packets were observed. This is acceptable for smoke, but traffic injection is required for PASS_RX_TRAFFIC."
    fi
} > "${OUT}"

cat "${OUT}"