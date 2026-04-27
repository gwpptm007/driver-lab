#!/usr/bin/env bash
# Common helpers for lab-vhost-user-basic.
# This lab verifies a DPDK vhost-user backend socket with testpmd.

set -euo pipefail

LAB_NAME="vhost-user-basic"
LAB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"
: "${HUGEPAGES:=1024}"
: "${HUGEPAGE_MOUNT:=/mnt/huge}"
: "${VHOST_SOCKET:=/tmp/dpdk-vhost-user0}"
: "${VHOST_QUEUES:=1}"
: "${VHOST_CLIENT_MODE:=0}"
: "${TESTPMD_RUNTIME:=18}"
: "${TESTPMD_CORES:=0-1}"
: "${TESTPMD_MEM_CHANNELS:=4}"
: "${TESTPMD_FILE_PREFIX:=vhost_basic}"
: "${TESTPMD_STATS_PERIOD:=2}"
: "${TESTPMD_FORWARD_MODE:=io}"

# Keep this lab independent from physical NIC binding. It should not touch ens192/ens33.
: "${NO_PCI:=1}"

timestamp() {
    date +"%Y%m%d_%H%M%S"
}

new_record_dir() {
    local ts
    ts="$(timestamp)"
    echo "${LAB_ROOT}/records/${ts}-${LAB_NAME}"
}

latest_record_dir() {
    local latest
    latest="$(find "${LAB_ROOT}/records" -maxdepth 1 -type d -name "*-${LAB_NAME}" 2>/dev/null | sort | tail -n 1 || true)"
    if [[ -n "${latest}" ]]; then
        echo "${latest}"
    else
        new_record_dir
    fi
}

ensure_record_dir() {
    if [[ -n "${RECORD_DIR:-}" ]]; then
        mkdir -p "${RECORD_DIR}"
        echo "${RECORD_DIR}"
        return
    fi

    local dir
    dir="$(latest_record_dir)"
    mkdir -p "${dir}"
    echo "${dir}"
}

need_cmd() {
    local cmd="$1"
    command -v "${cmd}" >/dev/null 2>&1
}

run_capture() {
    local out="$1"
    shift
    {
        echo "\$ $*"
        "$@"
    } >> "${out}" 2>&1 || {
        local rc=$?
        echo "[WARN] command failed rc=${rc}: $*" >> "${out}"
        return 0
    }
}

find_testpmd() {
    local candidates=(
        "${TESTPMD_BIN:-}"
        "dpdk-testpmd"
        "testpmd"
        "/usr/bin/dpdk-testpmd"
        "/usr/local/bin/dpdk-testpmd"
        "/opt/dpdk/build/app/dpdk-testpmd"
        "/opt/dpdk/build/app/testpmd"
    )

    local c
    for c in "${candidates[@]}"; do
        [[ -z "${c}" ]] && continue
        if [[ "${c}" == */* && -x "${c}" ]]; then
            echo "${c}"
            return 0
        fi
        if command -v "${c}" >/dev/null 2>&1; then
            command -v "${c}"
            return 0
        fi
    done
    return 1
}

require_root_for_write() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "ERROR: this action may modify hugepages/runtime sockets; please run with sudo." >&2
        exit 1
    fi
}

append_command_log() {
    local record_dir="$1"
    shift
    {
        echo
        echo "## $(date '+%F %T')"
        echo '```bash'
        printf '%q ' "$@"
        echo
        echo '```'
    } >> "${record_dir}/COMMANDS.md"
}

init_record_files() {
    local record_dir="$1"
    mkdir -p "${record_dir}"
    [[ -f "${record_dir}/COMMANDS.md" ]] || cat > "${record_dir}/COMMANDS.md" <<'EOC'
# COMMANDS

EOC
    [[ -f "${record_dir}/SUMMARY.md" ]] || cat > "${record_dir}/SUMMARY.md" <<EOF_SUMMARY
# SUMMARY

## Lab

lab-vhost-user-basic

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ${MGMT_IF}
- vhost-user socket: ${VHOST_SOCKET}

## 目标

- 复用 hugepage/testpmd 环境
- 使用 testpmd 启动 DPDK vhost-user backend
- 创建 UNIX domain socket
- 输出 testpmd port info/stats
- 不操作物理网卡 bind/unbind

## 结果

- 待填写

## 问题

- 待填写

## 下一步

- 待填写
EOF_SUMMARY
    [[ -f "${record_dir}/RESULT.md" ]] || cat > "${record_dir}/RESULT.md" <<'EOF_RESULT'
# RESULT

## Pass / Fail

待填写

## Evidence

待填写

## Review

待填写
EOF_RESULT
}

print_lab_env() {
    cat <<EOF_ENV
LAB_ROOT=${LAB_ROOT}
MGMT_IF=${MGMT_IF}
MGMT_PCI=${MGMT_PCI}
HUGEPAGES=${HUGEPAGES}
HUGEPAGE_MOUNT=${HUGEPAGE_MOUNT}
VHOST_SOCKET=${VHOST_SOCKET}
VHOST_QUEUES=${VHOST_QUEUES}
VHOST_CLIENT_MODE=${VHOST_CLIENT_MODE}
TESTPMD_RUNTIME=${TESTPMD_RUNTIME}
TESTPMD_CORES=${TESTPMD_CORES}
TESTPMD_MEM_CHANNELS=${TESTPMD_MEM_CHANNELS}
TESTPMD_FILE_PREFIX=${TESTPMD_FILE_PREFIX}
TESTPMD_STATS_PERIOD=${TESTPMD_STATS_PERIOD}
TESTPMD_FORWARD_MODE=${TESTPMD_FORWARD_MODE}
NO_PCI=${NO_PCI}
EOF_ENV
}

vhost_vdev_arg() {
    local arg="net_vhost0,iface=${VHOST_SOCKET},queues=${VHOST_QUEUES}"
    if [[ -n "${VHOST_CLIENT_MODE}" ]]; then
        arg="${arg},client=${VHOST_CLIENT_MODE}"
    fi
    echo "${arg}"
}

safe_socket_path_check() {
    case "${VHOST_SOCKET}" in
        /tmp/*|/var/run/*|/run/*)
            return 0
            ;;
        *)
            echo "ERROR: VHOST_SOCKET=${VHOST_SOCKET} is outside /tmp, /run or /var/run; refuse cleanup/start." >&2
            exit 2
            ;;
    esac
}
