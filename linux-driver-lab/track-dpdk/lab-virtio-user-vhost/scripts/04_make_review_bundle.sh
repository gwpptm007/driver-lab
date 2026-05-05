#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
append_command_log "${RECORD_DIR}" "$0" "$@"

OUT="${RECORD_DIR}/REVIEW_BUNDLE.md"
: > "${OUT}"

has_pattern() {
    local file="$1"
    local pattern="$2"
    [[ -f "${file}" ]] && grep -Eiq -- "${pattern}" "${file}"
}

has_nonzero_packets() {
    local file="$1"
    [[ -f "${file}" ]] || return 1
    awk '
        /RX-packets:|TX-packets:/ {
            for (i = 1; i <= NF; i++) {
                if ($i ~ /^[0-9]+$/ && $i > 0) found = 1
            }
        }
        END { exit found ? 0 : 1 }
    ' "${file}"
}

backend_log="${RECORD_DIR}/TESTPMD_BACKEND.log"
frontend_log="${RECORD_DIR}/TESTPMD_FRONTEND.log"
sock="${RECORD_DIR}/VHOST_SOCKET.txt"
post="${RECORD_DIR}/POST_CHECK.txt"
cmd="${RECORD_DIR}/TESTPMD_COMMANDS.txt"
runtime="${RECORD_DIR}/RUNTIME_STATUS.txt"

cmd_status="WARN"
if has_pattern "${cmd}" 'net_vhost0,iface=.*net_virtio_user0,path=|net_vhost0,iface=' && has_pattern "${cmd}" 'net_virtio_user0,path='; then
    cmd_status="PASS"
fi

socket_status="WARN"
if has_pattern "${sock}" 'socket_ready=1'; then
    socket_status="PASS"
fi

backend_status="WARN"
if has_pattern "${backend_log}" 'net_vhost|vhost|VHOST|Port [0-9]|Configuring Port|testpmd>'; then
    backend_status="PASS"
fi

frontend_status="WARN"
if has_pattern "${frontend_log}" 'virtio_user|net_virtio_user|virtio|Port [0-9]|Configuring Port|testpmd>'; then
    frontend_status="PASS"
fi

backend_stats_status="WARN"
if has_pattern "${backend_log}" 'show port stats|NIC statistics|port stats|RX-packets|TX-packets'; then
    backend_stats_status="PASS"
fi

frontend_stats_status="WARN"
if has_pattern "${frontend_log}" 'show port stats|NIC statistics|port stats|RX-packets|TX-packets'; then
    frontend_stats_status="PASS"
fi

traffic_status="WARN_ZERO_OR_UNKNOWN"
if has_nonzero_packets "${backend_log}" || has_nonzero_packets "${frontend_log}"; then
    traffic_status="PASS_NONZERO_PACKET_COUNTER"
fi

fatal_status="PASS"
if has_pattern "${backend_log}" 'EAL: FATAL|PANIC|failed|Error|Invalid argument' || has_pattern "${frontend_log}" 'EAL: FATAL|PANIC|failed|Error|Invalid argument'; then
    fatal_status="CHECK_LOG"
fi

cat > "${OUT}" <<'EOF_OUT'
# REVIEW_BUNDLE

## Lab

lab-virtio-user-vhost

## Review conclusion

| Item | Status | Evidence |
|------|--------|----------|
| backend/frontend commands generated | ${cmd_status} | TESTPMD_COMMANDS.txt |
| vhost-user socket created | ${socket_status} | VHOST_SOCKET.txt |
| backend net_vhost log available | ${backend_status} | TESTPMD_BACKEND.log |
| frontend virtio-user log available | ${frontend_status} | TESTPMD_FRONTEND.log |
| backend stats command executed | ${backend_stats_status} | TESTPMD_BACKEND.log |
| frontend stats command executed | ${frontend_stats_status} | TESTPMD_FRONTEND.log |
| packet counter non-zero | ${traffic_status} | TESTPMD_BACKEND.log / TESTPMD_FRONTEND.log |
| fatal/error quick scan | ${fatal_status} | TESTPMD_BACKEND.log / TESTPMD_FRONTEND.log |
| physical NIC untouched | PASS_BY_DESIGN | 两个 testpmd 均使用 --no-pci |

## Acceptance summary

本实验验证 DPDK 本机虚拟链路：

1. backend `net_vhost` 创建 `/tmp/dpdk-vhost-user0`。
2. frontend `net_virtio_user` 使用同一路径连接 backend。
3. 两边 testpmd 均输出 port info/stats。
4. 不操作真实物理网卡。

如果 packet counter 非零，说明 smoke test 进一步产生了可观察收发证据；如果为零但 backend/frontend/socket/stats 均 PASS，可以按 `PASS_WITH_WARN` 收口，再进入下一站自写 L2 app。

## Key files

- `TESTPMD_COMMANDS.txt`
- `TESTPMD_BACKEND.log`
- `TESTPMD_FRONTEND.log`
- `VHOST_SOCKET.txt`
- `RUNTIME_STATUS.txt`
- `POST_CHECK.txt`

## Next

进入：

`track-dpdk/lab-dpdk-l2-forwarding`
EOF_OUT

cat <<EOF_DONE
[OK] Review bundle saved:
${OUT}
EOF_DONE
