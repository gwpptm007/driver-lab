#!/usr/bin/env bash
#============================================================
# 09_make_review_bundle.sh — 生成测试评审报告
#
# 功能：
#   1. 检查本次实验所有必需文件是否存在
#   2. 检查各观测日志是否有非零统计（验证探针有触发）
#   3. 检查 XDP 清理状态
#   4. 生成 Markdown 格式的评审报告
#
# 评审标准：
#   - PASS_ENV: 环境检查文件存在
#   - PASS_PROBE_LIST: 探针列表文件存在
#   - PASS_TRACEPOINT_RX/TX/SOFTIRQ: 对应的观测日志存在且无错误
#   - KPROBE_OPTIONAL: 记录了 kprobe 结果（可能失败）
#   - Traffic Evidence: 如果观测日志有非零计数，标记为 YES
#
# 注意：
#   - tracepoint 是验收路径，kprobe 失败只记录为 NOTE
#   - 日志存在但计数器为零 = PASS_TRACEPOINT_SMOKE（观测基础设施正常，无流量）
#   - XDP 未清理 = WARN_ATTACHED_NOT_CLEANED
#
# 输出：
#   records/.../REVIEW_BUNDLE.md
#
# 使用：
#   ./scripts/09_make_review_bundle.sh
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

OUT_DIR="$(get_record_dir "${1:-}")"
OUT="${OUT_DIR}/REVIEW_BUNDLE.md"

# 辅助函数：检查文件是否存在且非空
exists() { [[ -s "${OUT_DIR}/$1" ]] && echo DONE || echo MISSING; }

# 辅助函数：检查日志是否正常（无 ERROR 和非零退出码）
log_ok() {
    local f="$1"
    [[ -s "${OUT_DIR}/${f}" ]] || { echo NO; return; }
    if grep -Eq 'ERROR:|BPFTRACE_RC=[1-9]' "${OUT_DIR}/${f}"; then echo NO; else echo YES; fi
}

# 辅助函数：检查日志是否有非零计数
traffic_seen() {
    local f="$1"
    [[ -s "${OUT_DIR}/${f}" ]] || { echo NO; return; }
    if grep -Eq '@[^:]*: [1-9][0-9]*|\[[^]]+\]: [1-9][0-9]*' "${OUT_DIR}/${f}"; then echo YES; else echo NO; fi
}

# 辅助函数：获取 XDP 清理状态
xdp_state() {
    if [[ -s "${OUT_DIR}/XDP_CLEAN.txt" ]]; then
        grep -E 'XDP_CLEAN_STATUS=' "${OUT_DIR}/XDP_CLEAN.txt" | tail -1 | cut -d= -f2- || echo UNKNOWN
    elif [[ -s "${OUT_DIR}/ENV_CHECK.txt" ]] && grep -q 'XDP_ATTACHED=YES' "${OUT_DIR}/ENV_CHECK.txt"; then
        echo WARN_ATTACHED_NOT_CLEANED
    else
        echo NOT_RECORDED
    fi
}

# 生成 Markdown 评审报告
cat > "${OUT}" <<EOF_REVIEW
# REVIEW_BUNDLE: ${LAB_NAME}

## Metadata

- Date: $(date -Is)
- Kernel: $(uname -r)
- Record dir: ${OUT_DIR}
- Interface hint: ${BPFTRACE_IFACE}
- Mode: tracepoint-first, kprobe optional, no BEGIN/END blocks

## Files

| File | Status |
|---|---|
| ENV_CHECK.txt | $(exists ENV_CHECK.txt) |
| PROBE_POINTS.txt | $(exists PROBE_POINTS.txt) |
| XDP_CLEAN.txt | $(exists XDP_CLEAN.txt) |
| RX_TRACEPOINT.log | $(exists RX_TRACEPOINT.log) |
| TX_TRACEPOINT.log | $(exists TX_TRACEPOINT.log) |
| SOFTIRQ_TRACEPOINT.log | $(exists SOFTIRQ_TRACEPOINT.log) |
| KPROBE_OPTIONAL.log | $(exists KPROBE_OPTIONAL.log) |
| COLLECT_STATS.txt | $(exists COLLECT_STATS.txt) |

## Judgement

| Item | Result |
|---|---|
| PASS_ENV | $( [[ -s "${OUT_DIR}/ENV_CHECK.txt" ]] && echo YES || echo NO ) |
| PASS_PROBE_LIST | $( [[ -s "${OUT_DIR}/PROBE_POINTS.txt" ]] && echo YES || echo NO ) |
| XDP_CLEAN_OR_WARNED | $(xdp_state) |
| PASS_TRACEPOINT_RX | $(log_ok RX_TRACEPOINT.log) |
| PASS_TRACEPOINT_TX | $(log_ok TX_TRACEPOINT.log) |
| PASS_SOFTIRQ | $(log_ok SOFTIRQ_TRACEPOINT.log) |
| KPROBE_OPTIONAL | $( if [[ -s "${OUT_DIR}/KPROBE_OPTIONAL.log" ]]; then echo RECORDED; else echo NOT_RUN; fi ) |

## Traffic Evidence

| Log | Non-zero evidence |
|---|---|
| RX_TRACEPOINT.log | $(traffic_seen RX_TRACEPOINT.log) |
| TX_TRACEPOINT.log | $(traffic_seen TX_TRACEPOINT.log) |
| SOFTIRQ_TRACEPOINT.log | $(traffic_seen SOFTIRQ_TRACEPOINT.log) |
| KPROBE_OPTIONAL.log | $(traffic_seen KPROBE_OPTIONAL.log) |

## Notes

- This lab now avoids bpftrace BEGIN/END blocks to bypass BEGIN_trigger/END_trigger compatibility issues.
- Tracepoints are the acceptance path because they are more stable across kernel builds than kprobes.
- kprobe failures caused by BTF/notrace/symbol differences should be recorded as NOTE, not as lab failure.
- If tracepoint logs exist but counters are zero, classify as PASS_TRACEPOINT_SMOKE but not PASS_TRAFFIC_OBSERVED.
- If ${BPFTRACE_IFACE} has an existing XDP program, skb-level tracepoints may not see packets; detach it with 02_clean_xdp_if_attached.sh when safe.
EOF_REVIEW

echo "REVIEW_BUNDLE=${OUT}"