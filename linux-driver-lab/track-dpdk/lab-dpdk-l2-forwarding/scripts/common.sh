#!/usr/bin/env bash
# Common helpers for lab-dpdk-l2-forwarding.
# This file is sourced by other scripts.

set -euo pipefail

LAB_NAME="dpdk-l2-forwarding"
LAB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIR="${LAB_ROOT}/app"
BUILD_DIR="${APP_DIR}/build"
APP_BIN="${BUILD_DIR}/l2fwd-lite"

: "${DPDK_IF:=ens192}"
: "${DPDK_PCI:=0000:0b:00.0}"
: "${DPDK_DRIVER:=uio_pci_generic}"
: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"
: "${HUGEPAGES:=1024}"
: "${HUGEPAGE_MOUNT:=/mnt/huge}"
: "${L2FWD_RUN_SECONDS:=15}"
: "${L2FWD_STATS_PERIOD:=2}"
: "${L2FWD_LCORES:=0-1}"
: "${L2FWD_MEMORY_CHANNELS:=4}"
: "${L2FWD_FILE_PREFIX:=l2fwd_lite}"
: "${L2FWD_BURST_SIZE:=32}"
: "${L2FWD_PROMISC:=1}"

# For optional two-port physical forwarding.
: "${DPDK_PCI_1:=}"

# For optional vdev smoke. net_null is commonly built in distro DPDK packages.
: "${L2FWD_VDEV0:=net_null0}"
: "${L2FWD_VDEV1:=net_null1}"

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

find_devbind() {
    local candidates=(
        "${DPDK_DEVBIND:-}"
        "dpdk-devbind.py"
        "dpdk-devbind"
        "/usr/share/dpdk/usertools/dpdk-devbind.py"
        "/usr/local/share/dpdk/usertools/dpdk-devbind.py"
        "/opt/dpdk/usertools/dpdk-devbind.py"
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
        echo "ERROR: this action modifies system state; please run with sudo." >&2
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
    [[ -f "${record_dir}/SUMMARY.md" ]] || cat > "${record_dir}/SUMMARY.md" <<EOF2
# SUMMARY

## Lab

lab-dpdk-l2-forwarding

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ${MGMT_IF}
- DPDK 网卡: ${DPDK_IF}
- DPDK PCI: ${DPDK_PCI}
- 默认 DPDK driver: ${DPDK_DRIVER}

## 目标

- 编译 l2fwd-lite
- 通过 EAL/mempool/ethdev/queue 初始化
- 进入 rx_burst/tx_burst 数据面循环
- 单端口 smoke 或双端口 L2 forwarding
- 输出软件 stats 与 rte_eth_stats

## 结果

- 待填写

## 问题

- 待填写

## 下一步

- 待填写
EOF2
    [[ -f "${record_dir}/RESULT.md" ]] || cat > "${record_dir}/RESULT.md" <<'EOR'
# RESULT

## Pass / Fail

待填写

## Evidence

待填写

## Review

待填写
EOR
}

guard_not_mgmt_pci() {
    if [[ "${DPDK_PCI}" == "${MGMT_PCI}" ]]; then
        echo "ERROR: DPDK_PCI=${DPDK_PCI} equals management PCI ${MGMT_PCI}; refuse to continue." >&2
        exit 2
    fi
    if [[ -n "${DPDK_PCI_1}" && "${DPDK_PCI_1}" == "${MGMT_PCI}" ]]; then
        echo "ERROR: DPDK_PCI_1=${DPDK_PCI_1} equals management PCI ${MGMT_PCI}; refuse to continue." >&2
        exit 2
    fi
}

print_lab_env() {
    cat <<EOF2
LAB_ROOT=${LAB_ROOT}
APP_DIR=${APP_DIR}
BUILD_DIR=${BUILD_DIR}
APP_BIN=${APP_BIN}
DPDK_IF=${DPDK_IF}
DPDK_PCI=${DPDK_PCI}
DPDK_PCI_1=${DPDK_PCI_1}
DPDK_DRIVER=${DPDK_DRIVER}
MGMT_IF=${MGMT_IF}
MGMT_PCI=${MGMT_PCI}
HUGEPAGES=${HUGEPAGES}
HUGEPAGE_MOUNT=${HUGEPAGE_MOUNT}
L2FWD_RUN_SECONDS=${L2FWD_RUN_SECONDS}
L2FWD_STATS_PERIOD=${L2FWD_STATS_PERIOD}
L2FWD_LCORES=${L2FWD_LCORES}
L2FWD_MEMORY_CHANNELS=${L2FWD_MEMORY_CHANNELS}
L2FWD_FILE_PREFIX=${L2FWD_FILE_PREFIX}
L2FWD_BURST_SIZE=${L2FWD_BURST_SIZE}
L2FWD_PROMISC=${L2FWD_PROMISC}
L2FWD_VDEV0=${L2FWD_VDEV0}
L2FWD_VDEV1=${L2FWD_VDEV1}
EOF2
}
