#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/REVIEW_BUNDLE.md"

has_file() { [[ -s "${RD}/$1" ]]; }
has_bad_bpftrace_error() {
    # 语法、命令缺失等属于脚本/环境硬错误。
    local f="$1"
    [[ -f "${RD}/${f}" ]] || return 1
    grep -Eiq 'syntax error|Could not resolve symbol: /proc/self/exe:BEGIN_trigger|stdin:|ERROR: Unknown|BPFTRACE_NOT_FOUND=1|RC=127' "${RD}/${f}"
}
has_attach_failure() {
    # attach 失败说明观测点不可用，不能当作 PASS。
    local f="$1"
    [[ -f "${RD}/${f}" ]] || return 1
    grep -Eiq 'cannot attach kprobe|ERROR: Error attaching probe|Invalid argument|RC=255' "${RD}/${f}"
}
has_untraceable_warn() {
    # 内核没有导出或禁止某个符号时，按 WARN 记录，便于和网络路径问题区分。
    local f="$1"
    [[ -f "${RD}/${f}" ]] || return 1
    grep -Eiq 'not traceable|NO_NAPI_POLL_.*AVAILABLE=1|NO_DRIVER_POLL_PROBES_AVAILABLE=1' "${RD}/${f}"
}
has_any_count() {
    # 只要任一 map 出现正计数，就说明捕获到了事件。
    local f="$1"
    [[ -f "${RD}/${f}" ]] || return 1
    grep -Eq '@[^:]+:[[:space:]]*[1-9][0-9]*|\[[^]]+\]:[[:space:]]*[1-9][0-9]*' "${RD}/${f}"
}
status_file() {
    local f="$1"
    if has_file "$f"; then echo "DONE"; else echo "MISSING"; fi
}
pass_log() {
    # 统一把日志文件折算成 review bundle 里的判定值。
    local f="$1"
    if ! has_file "$f"; then echo "NO_MISSING"; return; fi
    if has_bad_bpftrace_error "$f"; then echo "NO_ERROR"; return; fi
    if has_attach_failure "$f"; then echo "NO_ATTACH_FAILED"; return; fi
    if has_untraceable_warn "$f"; then echo "WARN_NOT_TRACEABLE"; return; fi
    echo "YES"
}

PASS_ENV=NO
has_file ENV_CHECK.txt && PASS_ENV=YES
PASS_PROBES=NO
has_file NAPI_PROBE_POINTS.txt && PASS_PROBES=YES
PASS_NAPI_KPROBE=$(pass_log NAPI_POLL_KPROBE.log)
PASS_NAPI_RETPROBE=$(pass_log NAPI_POLL_RETPROBE.log)
PASS_DRIVER_OPTIONAL=$(pass_log DRIVER_POLL_OPTIONAL.log)
PASS_SOFTIRQ=$(pass_log SOFTIRQ_NAPI_CORRELATION.log)
TRAFFIC_OBSERVED=NO
for f in NAPI_POLL_KPROBE.log NAPI_POLL_RETPROBE.log DRIVER_POLL_OPTIONAL.log SOFTIRQ_NAPI_CORRELATION.log; do
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
    for f in ENV_CHECK.txt NAPI_PROBE_POINTS.txt NAPI_POLL_KPROBE.log NAPI_POLL_RETPROBE.log DRIVER_POLL_OPTIONAL.log SOFTIRQ_NAPI_CORRELATION.log COLLECT_STATS.txt; do
        echo "| ${f} | $(status_file "$f") |"
    done
    echo
    echo "## 判定"
    echo
    echo "| item | result |"
    echo "|---|---|"
    echo "| PASS_ENV | ${PASS_ENV} |"
    echo "| PASS_PROBE_LIST | ${PASS_PROBES} |"
    echo "| PASS_NAPI_KPROBE | ${PASS_NAPI_KPROBE} |"
    echo "| PASS_NAPI_RETPROBE | ${PASS_NAPI_RETPROBE} |"
    echo "| DRIVER_POLL_OPTIONAL | ${PASS_DRIVER_OPTIONAL} |"
    echo "| PASS_SOFTIRQ_CORRELATION | ${PASS_SOFTIRQ} |"
    echo "| TRAFFIC_OR_EVENTS_OBSERVED | ${TRAFFIC_OBSERVED} |"
    echo
    echo "## 结论建议"
    echo
    if [[ "${PASS_ENV}" == "YES" && "${PASS_PROBES}" == "YES" && "${PASS_NAPI_KPROBE}" == "YES" && "${PASS_SOFTIRQ}" == "YES" ]]; then
        if [[ "${TRAFFIC_OBSERVED}" == "YES" ]]; then
            echo "PASS_NAPI_OBSERVE"
        else
            echo "PASS_ATTACH_NO_TRAFFIC"
        fi
    elif [[ "${PASS_ENV}" == "YES" && "${PASS_PROBES}" == "YES" && "${PASS_SOFTIRQ}" == "YES" && "${TRAFFIC_OBSERVED}" == "YES" ]]; then
        echo "WARN_PARTIAL_NAPI_OBSERVE"
    else
        echo "NEED_RETEST_OR_FIX"
    fi
    echo
    echo "## 说明"
    echo
    echo "驱动 poll 观测是 optional。不同内核和驱动符号可能不同，失败时不阻塞最低验收。"
} | tee "${OUT}"
echo "REVIEW_BUNDLE=${OUT}"
