#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/REVIEW_BUNDLE.md"
append_command_log "${RECORD_DIR}" "$0"

status_for() {
    local file="$1"
    if [[ -s "${RECORD_DIR}/${file}" ]]; then
        echo "DONE"
    else
        echo "MISSING"
    fi
}

has_pattern() {
    local pattern="$1"
    if grep -R -q -- "${pattern}" "${RECORD_DIR}" 2>/dev/null; then
        echo "YES"
    else
        echo "NO"
    fi
}

# Pre-compute all values before heredoc
val_env=$(status_for ENV_CHECK.txt)
val_build=$(status_for BUILD.log)
val_prepare=$(status_for PREPARE_VMXNET3.txt)
val_single=$(status_for FASTPATH_SINGLE_PORT.log)
val_two=$(status_for FASTPATH_TWO_PORT.log)
val_null=$(status_for FASTPATH_VDEV_NULL_PAIR.log)
val_rewrite=$(status_for FASTPATH_REWRITE_DEMO.log)
val_stats=$(status_for COLLECT_STATS.txt)

ev_config=$(has_pattern 'fastpath-lite config')
ev_policy=$(has_pattern 'policy: promisc')
ev_rewrite=$(has_pattern 'rewrite rules')
ev_started=$(has_pattern 'port .* started')
ev_ports=$(has_pattern 'available/initialized ports')
ev_loop=$(has_pattern 'enter fastpath loop')
ev_swstats=$(has_pattern 'fastpath-lite software stats')
ev_ethstats=$(has_pattern 'rte_eth_stats')
ev_bye=$(has_pattern 'bye')

cat > "${OUT}" <<EOF
# REVIEW_BUNDLE - project-user-space-fastpath

## Record directory

\`${RECORD_DIR}\`

## Checklist

| Item | Status |
|---|---|
| ENV_CHECK.txt | ${val_env} |
| BUILD.log | ${val_build} |
| PREPARE_VMXNET3.txt | ${val_prepare} |
| FASTPATH_SINGLE_PORT.log | ${val_single} |
| FASTPATH_TWO_PORT.log | ${val_two} |
| FASTPATH_VDEV_NULL_PAIR.log | ${val_null} |
| FASTPATH_REWRITE_DEMO.log | ${val_rewrite} |
| COLLECT_STATS.txt | ${val_stats} |

## Evidence grep

| Evidence | Found |
|---|---|
| fastpath-lite config | ${ev_config} |
| policy: promisc | ${ev_policy} |
| rewrite rules | ${ev_rewrite} |
| port started | ${ev_started} |
| available/initialized ports | ${ev_ports} |
| enter fastpath loop | ${ev_loop} |
| fastpath-lite software stats | ${ev_swstats} |
| rte_eth_stats | ${ev_ethstats} |
| bye | ${ev_bye} |

## Suggested verdict

- \`PASS_SMOKE\`: BUILD + one of SINGLE_PORT/VDEV_NULL_PAIR succeeds and logs contain init/stats/bye.
- \`PASS_PROJECT\`: above plus UDP-only/rewrite demo logs show policy and rewrite rules.
- \`PASS_FORWARDING\`: two physical/vhost/virtio ports receive external traffic and tx/rx counters increase.

## Reviewer notes

1. 当前 VMware 测试机只有一个专用 VMXNET3 DPDK 口时，先按 \`PASS_SMOKE\` 验收。
2. 有两个 DPDK 端口或接入 vhost/virtio-user 后，再按 \`PASS_FORWARDING\` 验收。
3. \`rx=0/tx=0\` 不自动判失败；没有外部发包源时，只能证明初始化和 loop，不证明转发吞吐。
EOF

echo "[OK] review bundle saved: ${OUT}"