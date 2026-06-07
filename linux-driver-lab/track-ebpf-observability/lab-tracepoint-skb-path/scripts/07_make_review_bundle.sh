#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/REVIEW_BUNDLE.md"

has_file() { [[ -s "${RD}/$1" ]]; }
has_bad_bpftrace_error() {
    local f="$1"
    [[ -f "${RD}/${f}" ]] || return 1
    grep -Eiq 'syntax error|Could not resolve symbol|stdin:|ERROR: Unknown|BPFTRACE_NOT_FOUND=1|RC=127' "${RD}/${f}"
}
has_attach_failure() {
    local f="$1"
    [[ -f "${RD}/${f}" ]] || return 1
    grep -Eiq 'cannot attach|ERROR: Error attaching|Invalid argument|RC=255' "${RD}/${f}"
}
has_untraceable_warn() {
    local f="$1"
    [[ -f "${RD}/${f}" ]] || return 1
    grep -Eiq 'not traceable|NO_.*_AVAILABLE=1' "${RD}/${f}"
}
has_any_count() {
    local f="$1"
    [[ -f "${RD}/${f}" ]] || return 1
    grep -Eq '@[^:]+:[[:space:]]*[1-9][0-9]*|\[[^]]+\]:[[:space:]]*[1-9][0-9]*|[0-9]$|[0-9][[:space:]]+$' "${RD}/${f}"
}
status_file() {
    local f="$1"
    if has_file "$f"; then echo "DONE"; else echo "MISSING"; fi
}
pass_log() {
    local f="$1"
    if ! has_file "$f"; then echo "NO_MISSING"; return; fi
    if has_bad_bpftrace_error "$f"; then echo "NO_ERROR"; return; fi
    if has_attach_failure "$f"; then echo "NO_ATTACH_FAILED"; return; fi
    if has_untraceable_warn "$f"; then echo "WARN_NOT_TRACEABLE"; return; fi
    echo "YES"
}

PASS_ENV=NO
has_file ENV_CHECK.txt && PASS_ENV=YES
PASS_TPS=NO
has_file TRACEPOINT_LIST.txt && PASS_TPS=YES
PASS_RX=$(pass_log SKB_RX_TRACE.log)
PASS_TX=$(pass_log SKB_TX_TRACE.log)
PASS_DROP=$(pass_log SKB_DROP_TRACE.log)
PASS_FULL=$(pass_log SKB_FULL_PATH.log)
TRAFFIC_OBSERVED=NO
for f in SKB_RX_TRACE.log SKB_TX_TRACE.log SKB_DROP_TRACE.log SKB_FULL_PATH.log; do
    if has_any_count "$f"; then TRAFFIC_OBSERVED=YES; fi
done

{
    echo "# REVIEW_BUNDLE - ${LAB_NAME}"
    echo
    echo "- record_dir: \`${RD}\`"
    echo "- date: $(date -Iseconds)"
    echo
    echo "## 文件状态"
    echo
    echo "| file | status |"
    echo "|---|---|"
    for f in ENV_CHECK.txt TRACEPOINT_LIST.txt SKB_RX_TRACE.log SKB_TX_TRACE.log SKB_DROP_TRACE.log SKB_FULL_PATH.log COLLECT_STATS.txt; do
        echo "| ${f} | $(status_file "$f") |"
    done
    echo
    echo "## 判定"
    echo
    echo "| item | result |"
    echo "|---|---|"
    echo "| PASS_ENV | ${PASS_ENV} |"
    echo "| PASS_TRACEPOINT_LIST | ${PASS_TPS} |"
    echo "| PASS_RX_TRACE | ${PASS_RX} |"
    echo "| PASS_TX_TRACE | ${PASS_TX} |"
    echo "| PASS_DROP_TRACE | ${PASS_DROP} |"
    echo "| PASS_FULL_PATH | ${PASS_FULL} |"
    echo "| TRAFFIC_OR_EVENTS_OBSERVED | ${TRAFFIC_OBSERVED} |"
    echo
    echo "## tracepoint vs kprobe 对照说明"
    echo
    echo "| 维度 | kprobe | tracepoint |"
    echo "|---|---|---|"
    echo "| 稳定性 | 函数名变化导致不兼容 | 内核 ABI，跨版本稳定 |"
    echo "| 字段访问 | 需要知道结构体布局 | args->字段 可直接访问 |"
    echo "| 适用场景 | 无 tracepoint 的深层路径 | skb 层面首选 tracepoint |"
    echo "| 上游保证 | 无 | tracepoint 变更视为 ABI break |"
    echo
    echo "## 结论建议"
    echo
    if [[ "${PASS_ENV}" == "YES" && "${PASS_TPS}" == "YES" && "${PASS_RX}" == "YES" && "${PASS_TX}" == "YES" ]]; then
        if [[ "${TRAFFIC_OBSERVED}" == "YES" ]]; then
            echo "PASS_SKB_TRACEPOINT_OBSERVE"
        else
            echo "PASS_ATTACH_NO_TRAFFIC"
        fi
    elif [[ "${PASS_ENV}" == "YES" && "${TRAFFIC_OBSERVED}" == "YES" ]]; then
        echo "WARN_PARTIAL_SKB_TRACEPOINT"
    else
        echo "NEED_RETEST_OR_FIX"
    fi
    echo
    echo "## 说明"
    echo
    echo "drop trace (kfree_skb) 和 full-path 合并观测是加分项。RX+TX 双通是最低验收。"
    echo "Phase 3 相比 Phase 2 的进步：从 kprobe 函数级观测 → tracepoint ABI 级观测，稳定性大幅提升。"
} | tee "${OUT}"
echo "REVIEW_BUNDLE=${OUT}"
