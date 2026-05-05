#!/usr/bin/env bash
# Common helpers for project-user-space-fastpath.

set -euo pipefail

PROJECT_NAME="user-space-fastpath"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIR="${PROJECT_ROOT}/app"
BUILD_DIR="${APP_DIR}/build"
APP_BIN="${BUILD_DIR}/fastpath-lite"

: "${DPDK_IF:=ens192}"
: "${DPDK_PCI:=0000:0b:00.0}"
: "${DPDK_PCI_1:=}"
: "${DPDK_DRIVER:=uio_pci_generic}"
: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"
: "${HUGEPAGES:=1024}"
: "${HUGEPAGE_MOUNT:=/mnt/huge}"

: "${FASTPATH_LCORES:=0-1}"
: "${FASTPATH_MEMORY_CHANNELS:=4}"
: "${FASTPATH_FILE_PREFIX:=fastpath_lite}"
: "${FASTPATH_RUN_SECONDS:=20}"
: "${FASTPATH_STATS_PERIOD:=2}"
: "${FASTPATH_BURST_SIZE:=32}"
: "${FASTPATH_PROMISC:=1}"
: "${FASTPATH_UDP_ONLY:=1}"
: "${FASTPATH_SWAP_MAC:=1}"
: "${FASTPATH_REWRITE_ENABLE:=0}"
: "${FASTPATH_EXTRA_APP_ARGS:=}"

# Optional vdev smoke. net_null is common in distro DPDK packages.
: "${FASTPATH_VDEV0:=net_null0}"
: "${FASTPATH_VDEV1:=net_null1}"

# Optional external traffic hint; scripts do not generate traffic by default.
: "${TRAFFIC_HINT:=Use another VM/host/scapy/pktgen to send UDP packets into the DPDK port.}"

timestamp() {
    date +"%Y%m%d_%H%M%S"
}

new_record_dir() {
    local ts
    ts="$(timestamp)"
    echo "${PROJECT_ROOT}/records/${ts}-${PROJECT_NAME}"
}

latest_record_dir() {
    local latest
    latest="$(find "${PROJECT_ROOT}/records" -maxdepth 1 -type d -name "*-${PROJECT_NAME}" 2>/dev/null | sort | tail -n 1 || true)"
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
    command -v "$1" >/dev/null 2>&1
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

print_project_env() {
    cat <<EOF2
PROJECT_ROOT=${PROJECT_ROOT}
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
FASTPATH_LCORES=${FASTPATH_LCORES}
FASTPATH_MEMORY_CHANNELS=${FASTPATH_MEMORY_CHANNELS}
FASTPATH_FILE_PREFIX=${FASTPATH_FILE_PREFIX}
FASTPATH_RUN_SECONDS=${FASTPATH_RUN_SECONDS}
FASTPATH_STATS_PERIOD=${FASTPATH_STATS_PERIOD}
FASTPATH_BURST_SIZE=${FASTPATH_BURST_SIZE}
FASTPATH_PROMISC=${FASTPATH_PROMISC}
FASTPATH_UDP_ONLY=${FASTPATH_UDP_ONLY}
FASTPATH_SWAP_MAC=${FASTPATH_SWAP_MAC}
FASTPATH_REWRITE_ENABLE=${FASTPATH_REWRITE_ENABLE}
FASTPATH_EXTRA_APP_ARGS=${FASTPATH_EXTRA_APP_ARGS}
FASTPATH_VDEV0=${FASTPATH_VDEV0}
FASTPATH_VDEV1=${FASTPATH_VDEV1}
EOF2
}

init_record_files() {
    local record_dir="$1"
    mkdir -p "${record_dir}"
    [[ -f "${record_dir}/COMMANDS.md" ]] || cat > "${record_dir}/COMMANDS.md" <<'EOC'
# COMMANDS

EOC
    [[ -f "${record_dir}/SUMMARY.md" ]] || cat > "${record_dir}/SUMMARY.md" <<EOF2
# SUMMARY

## Project

project-user-space-fastpath

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ${MGMT_IF}
- DPDK 网卡: ${DPDK_IF}
- DPDK PCI: ${DPDK_PCI}
- 默认 DPDK driver: ${DPDK_DRIVER}

## 目标

- 编译 fastpath-lite
- 初始化 DPDK EAL / mempool / ethdev / queue
- 进入 poll-mode fastpath loop
- 识别 ARP / IPv4 / UDP / non-UDP
- 可选 UDP-only 过滤
- 可选 MAC / IPv4 / UDP port rewrite
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

setup_hugepages() {
    mkdir -p "${HUGEPAGE_MOUNT}"
    if ! mountpoint -q "${HUGEPAGE_MOUNT}"; then
        mount -t hugetlbfs nodev "${HUGEPAGE_MOUNT}"
    fi
    echo "${HUGEPAGES}" > /proc/sys/vm/nr_hugepages
}

base_app_args() {
    printf '%s ' \
        --run-seconds "${FASTPATH_RUN_SECONDS}" \
        --stats-period "${FASTPATH_STATS_PERIOD}" \
        --burst-size "${FASTPATH_BURST_SIZE}" \
        --promisc "${FASTPATH_PROMISC}" \
        --udp-only "${FASTPATH_UDP_ONLY}" \
        --swap-mac "${FASTPATH_SWAP_MAC}" \
        --rewrite "${FASTPATH_REWRITE_ENABLE}"
    if [[ -n "${FASTPATH_EXTRA_APP_ARGS}" ]]; then
        printf '%s ' ${FASTPATH_EXTRA_APP_ARGS}
    fi
}

require_app_bin() {
    if [[ ! -x "${APP_BIN}" ]]; then
        echo "ERROR: APP_BIN not found or not executable: ${APP_BIN}" >&2
        echo "Run: ./scripts/01_build_app.sh" >&2
        exit 1
    fi
}
