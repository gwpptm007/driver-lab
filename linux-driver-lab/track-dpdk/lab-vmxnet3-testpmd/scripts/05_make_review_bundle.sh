#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

BUNDLE="${RECORD_DIR}/REVIEW_BUNDLE.md"
EXEC_BOARD="${LAB_ROOT}/reports/lab-vmxnet3-testpmd_exec_board.md"

status_of() {
    local file="$1"
    if [[ -s "${RECORD_DIR}/${file}" ]]; then
        echo "DONE"
    else
        echo "MISSING"
    fi
}

cat > "${BUNDLE}" <<EOF
# REVIEW_BUNDLE

## Lab

lab-vmxnet3-testpmd

## Record directory

\`${RECORD_DIR}\`

## Expected test machine

| Item | Value |
|------|-------|
| Guest | Ubuntu 22.04.5 Desktop |
| Kernel | Linux 6.8.0-110-generic |
| Management NIC | ${MGMT_IF} / e1000 |
| DPDK NIC | ${DPDK_IF} / vmxnet3 |
| DPDK PCI | ${DPDK_PCI} |
| DPDK Driver | ${DPDK_DRIVER} |

## Evidence checklist

| Evidence | Status |
|----------|--------|
| ENV_CHECK.txt | $(status_of ENV_CHECK.txt) |
| HUGEPAGE_SETUP.txt | $(status_of HUGEPAGE_SETUP.txt) |
| BIND_BEFORE.txt | $(status_of BIND_BEFORE.txt) |
| BIND_AFTER.txt | $(status_of BIND_AFTER.txt) |
| TESTPMD.log | $(status_of TESTPMD.log) |
| BIND_STATUS.txt | $(status_of BIND_STATUS.txt) |
| HUGEPAGE_STATUS.txt | $(status_of HUGEPAGE_STATUS.txt) |
| PCI_DETAIL.txt | $(status_of PCI_DETAIL.txt) |
| DMESG_DPDK_NET.txt | $(status_of DMESG_DPDK_NET.txt) |

## Review questions

1. ens33 是否保持管理链路？
2. ens192/0000:0b:00.0 是否为本轮唯一 DPDK 测试口？
3. hugepage 是否有可用页？
4. bind 前后 driver 是否符合预期？
5. testpmd 是否完成 EAL/PMD/port/stats 输出？
6. 失败项是否有完整日志？

## Next

通过后进入：

\`track-dpdk/lab-vhost-user-basic\`
EOF

cat > "${EXEC_BOARD}" <<EOF
# lab-vmxnet3-testpmd_exec_board

| Phase | 项目 | 状态 | 证据 |
|------|------|------|------|
| P0 | 测试机环境对齐 | 待测试机确认 | docs/04_TEST_MACHINE_ENV.md |
| P1 | 只读环境检查 | $(status_of ENV_CHECK.txt) | records/*/ENV_CHECK.txt |
| P2 | hugepage 配置 | $(status_of HUGEPAGE_SETUP.txt) | records/*/HUGEPAGE_SETUP.txt |
| P3 | vmxnet3 bind | $(status_of BIND_AFTER.txt) | records/*/BIND_AFTER.txt |
| P4 | testpmd smoke | $(status_of TESTPMD.log) | records/*/TESTPMD.log |
| P5 | stats 收集 | $(status_of BIND_STATUS.txt) | records/*/BIND_STATUS.txt |
| P6 | review bundle | DONE | records/*/REVIEW_BUNDLE.md |

## 默认测试机

- Guest: Ubuntu 22.04.5 Desktop / Linux 6.8.0-110-generic
- 管理口: ens33 / e1000 / 192.168.65.135
- DPDK口: ens192 / vmxnet3 / 0000:0b:00.0
EOF

echo "[OK] Review bundle generated:"
echo "  ${BUNDLE}"
echo "  ${EXEC_BOARD}"
