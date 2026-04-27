#!/usr/bin/env bash
# Common helpers for lab-vmxnet3-testpmd.
# This file is sourced by other scripts.

set -euo pipefail

LAB_NAME="vmxnet3-testpmd"
LAB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${DPDK_IF:=ens192}"
: "${DPDK_PCI:=0000:0b:00.0}"
: "${DPDK_DRIVER:=vfio-pci}"
: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"
: "${HUGEPAGES:=1024}"
: "${HUGEPAGE_MOUNT:=/mnt/huge}"

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
    [[ -f "${record_dir}/COMMANDS.md" ]] || cat > "${record_dir}/COMMANDS.md" <<'EOF'
# COMMANDS

EOF
    [[ -f "${record_dir}/SUMMARY.md" ]] || cat > "${record_dir}/SUMMARY.md" <<EOF
# SUMMARY

## Lab

lab-vmxnet3-testpmd

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ${MGMT_IF}
- DPDK 网卡: ${DPDK_IF}
- DPDK PCI: ${DPDK_PCI}

## 目标

- hugepage
- vfio/uio bind
- testpmd
- port stats
- records/report

## 结果

- 待填写

## 问题

- 待填写

## 下一步

- 待填写
EOF
    [[ -f "${record_dir}/RESULT.md" ]] || cat > "${record_dir}/RESULT.md" <<'EOF'
# RESULT

## Pass / Fail

待填写

## Evidence

待填写

## Review

待填写
EOF
}

guard_not_mgmt_pci() {
    if [[ "${DPDK_PCI}" == "${MGMT_PCI}" ]]; then
        echo "ERROR: DPDK_PCI=${DPDK_PCI} equals management PCI ${MGMT_PCI}; refuse to continue." >&2
        exit 2
    fi
}

print_lab_env() {
    cat <<EOF
LAB_ROOT=${LAB_ROOT}
DPDK_IF=${DPDK_IF}
DPDK_PCI=${DPDK_PCI}
DPDK_DRIVER=${DPDK_DRIVER}
MGMT_IF=${MGMT_IF}
MGMT_PCI=${MGMT_PCI}
HUGEPAGES=${HUGEPAGES}
HUGEPAGE_MOUNT=${HUGEPAGE_MOUNT}
EOF
}
