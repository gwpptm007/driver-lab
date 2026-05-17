#!/usr/bin/env bash
# 脚本: 04_make_review_bundle.sh
# 功能: 汇总所有记录文件，检查 socket 创建、日志、统计信息，生成审查报告
# 用法: ./scripts/04_make_review_bundle.sh

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/REVIEW_BUNDLE.md"
: > "${OUT}"

# 辅助函数：检查记录文件中是否包含指定关键字
# 用于判断该步骤是否成功完成（如 socket 是否创建、日志是否有统计信息）
has_pattern() {
    local file="$1"
    local pattern="$2"
    [[ -f "${file}" ]] && grep -Eiq "${pattern}" "${file}"
}

# 各记录文件路径
log="${RECORD_DIR}/TESTPMD_VHOST.log"
sock="${RECORD_DIR}/VHOST_SOCKET.txt"
post="${RECORD_DIR}/POST_CHECK.txt"
cmd="${RECORD_DIR}/TESTPMD_COMMAND.txt"

# 判断 vhost socket 是否成功创建（socket_ready=1）
socket_status="WARN"
if has_pattern "${sock}" 'socket_ready=1'; then
    socket_status="PASS"
fi

# 判断 vhost/testpmd 日志是否包含预期关键字（vhost/net_vhost/Port 等）
vhost_log_status="WARN"
if has_pattern "${log}" 'vhost|net_vhost|VHOST|Port [0-9]|Configuring Port|testpmd'; then
    vhost_log_status="PASS"
fi

# 判断是否执行了 stats 命令（RX-packets/TX-packets 等统计信息）
stats_status="WARN"
if has_pattern "${log}" 'show port stats|NIC statistics|port stats|RX-packets|TX-packets'; then
    stats_status="PASS"
fi

cat > "${OUT}" <<EOF_OUT
# REVIEW_BUNDLE

## Lab

lab-vhost-user-basic

## Review conclusion

| Item | Status | Evidence |
|------|--------|----------|
| testpmd command generated | $([[ -f "${cmd}" ]] && echo PASS || echo WARN) | TESTPMD_COMMAND.txt |
| vhost-user socket created | ${socket_status} | VHOST_SOCKET.txt |
| vhost/testpmd log available | ${vhost_log_status} | TESTPMD_VHOST.log |
| port/stats command executed | ${stats_status} | TESTPMD_VHOST.log |
| physical NIC untouched | PASS_BY_DESIGN | 本实验使用 --no-pci，不执行 bind/unbind |

## Acceptance summary

本实验只验证 DPDK vhost-user backend 的最小闭环：

1. `dpdk-testpmd` 通过 `--vdev=net_vhost0,iface=...` 启动 vhost-user backend。
2. 运行期间创建 UNIX domain socket。
3. `testpmd` 可以进入 forwarding/stats 流程。
4. 不要求有 virtio peer，不要求 RX/TX 非 0。

## Next step

进入：`track-dpdk/lab-virtio-user-vhost`

该下一站会用 `net_virtio_user` 或等价方式连接本实验创建的 vhost-user socket，形成本机 backend/frontend 对接。
EOF_OUT

cat > "${RECORD_DIR}/RESULT.md" <<EOF_RESULT
# RESULT

## Pass / Fail

待测试机填写。若 REVIEW_BUNDLE 中 socket/log/stats 均为 PASS，则本 lab 判定 PASS。

## Evidence

- TESTPMD_COMMAND.txt
- TESTPMD_VHOST.log
- VHOST_SOCKET.txt
- RUNTIME_STATUS.txt
- POST_CHECK.txt
- REVIEW_BUNDLE.md

## Review

本 lab 不依赖 ens192/vmxnet3，也不应修改管理口 ens33。重点看 vhost-user socket 是否在 testpmd 运行期间被创建。
EOF_RESULT

cat <<EOF_DONE
[OK] Review bundle generated:
${OUT}
EOF_DONE
