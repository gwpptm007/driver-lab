#!/usr/bin/env bash
# =============================================================================
# 07_make_review_bundle.sh — 生成评审包（只读，不修改系统）
#
# 汇总所有实验证据，生成一份 REVIEW_BUNDLE.md，包含：
#   1. 记录目录路径
#   2. Checklist 表格：每个产出文件是否存在
#   3. 关键证据提取：
#      - 编译结果（从 BUILD.log）
#      - 端口初始化（从运行日志）
#      - 转发循环（从运行日志）
#   4. PASS 判定标准：
#      - PASS_SMOKE：单端口，编译+初始化+转发循环+stats
#      - PASS_FORWARDING：双端口，rx/tx 非 0
#
# 产出：records/<timestamp>-dpdk-l2-forwarding/REVIEW_BUNDLE.md
# =============================================================================
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/REVIEW_BUNDLE.md"
: > "${OUT}"
append_command_log "${RECORD_DIR}" "$0"

# ── 辅助函数 ─────────────────────────────────────────────────────────────────

# 检查文件是否存在且非空，返回 DONE 或 MISSING
status_for_file() {
    local f="$1"
    if [[ -s "${RECORD_DIR}/${f}" ]]; then
        echo "DONE"
    else
        echo "MISSING"
    fi
}

# 从文件中提取第一个匹配的行（用于关键证据提取）
extract_first_match() {
    local pattern="$1"
    shift
    grep -Rhm1 -E "${pattern}" "$@" 2>/dev/null || true
}

# ── 定位各运行日志 ────────────────────────────────────────────────────────────
single_log="${RECORD_DIR}/L2FWD_SINGLE_PORT.log"
two_log="${RECORD_DIR}/L2FWD_TWO_PORT.log"
null_log="${RECORD_DIR}/L2FWD_VDEV_NULL_PAIR.log"

# ── 提取关键证据 ─────────────────────────────────────────────────────────────
# 编译证据：ninja 输出、链接、二进制文件信息、或错误
build_evidence=$(extract_first_match 'ninja:|Linking target|binary|l2fwd-lite|error|failed' "${RECORD_DIR}/BUILD.log" 2>/dev/null || echo "none")

# 端口初始化证据：成功初始化的端口数、单端口通知、或失败信息
port_evidence=$(extract_first_match 'available/initialized ports|port [0-9]+ started|notice: only one port|no available DPDK|failed' "${single_log}" "${two_log}" "${null_log}" 2>/dev/null || echo "none")

# 转发循环证据：进入循环、定时退出、统计输出、或错误
fwd_evidence=$(extract_first_match 'enter forwarding loop|run_seconds reached|software stats|rte_eth_stats|bye|failed|error' "${single_log}" "${two_log}" "${null_log}" 2>/dev/null || echo "none")

# ── 生成评审报告 ─────────────────────────────────────────────────────────────
cat > "${OUT}" <<EOF
# REVIEW_BUNDLE - lab-dpdk-l2-forwarding

## 1. Record directory

\`${RECORD_DIR}\`

## 2. Checklist

| Item | Status |
|---|---|
| ENV_CHECK.txt | $(status_for_file ENV_CHECK.txt) |
| BUILD.log | $(status_for_file BUILD.log) |
| PREPARE_VMXNET3.txt | $(status_for_file PREPARE_VMXNET3.txt) |
| L2FWD_SINGLE_PORT.log | $(status_for_file L2FWD_SINGLE_PORT.log) |
| L2FWD_TWO_PORT.log | $(status_for_file L2FWD_TWO_PORT.log) |
| L2FWD_VDEV_NULL_PAIR.log | $(status_for_file L2FWD_VDEV_NULL_PAIR.log) |
| COLLECT_STATS.txt | $(status_for_file COLLECT_STATS.txt) |

## 3. Key evidence

### Build

\`\`\`
${build_evidence}
\`\`\`

### Port initialization

\`\`\`
${port_evidence}
\`\`\`

### Forwarding loop

\`\`\`
${fwd_evidence}
\`\`\`

## 4. PASS criteria

### PASS_SMOKE

满足以下条件即可判定本 lab 在当前单 VMXNET3 测试机上通过：

- \`BUILD.log\` 显示 \`l2fwd-lite\` 编译成功。
- \`L2FWD_SINGLE_PORT.log\` 中出现 \`available/initialized ports: 1\` 或更多。
- 日志中出现 \`enter forwarding loop\`。
- 日志中出现 \`rte_eth_stats\` 和 \`bye\`。

### PASS_FORWARDING

如果后续接入两个 DPDK 端口或外部发包源，进一步看：

- 至少两个端口初始化成功。
- \`rx\` / \`tx\` / \`ipackets\` / \`opackets\` 不全为 0。
- 没有持续增长的 \`tx_failed\`。

## 5. Reviewer notes

- 当前测试机只有一个 VMXNET3 DPDK 口，所以单端口场景主要验证 C app 数据面骨架，不强制 RX/TX 非 0。
- 真正互转需要第二个 DPDK 口、vhost/virtio-user 拓扑或外部发包器。
EOF

echo "[OK] review bundle: ${OUT}"
