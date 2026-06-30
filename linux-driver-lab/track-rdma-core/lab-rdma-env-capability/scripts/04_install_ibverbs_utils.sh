#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(ensure_record_dir)"
log="${record_dir}/INSTALL_IBVERBS_UTILS.log"

{
    printf '# Install ibverbs-utils\n'
    printf 'record_dir=%s\n' "${record_dir}"
    printf 'timestamp=%s\n' "$(date -Is)"

    run_or_note "dpkg package status before" sh -c "dpkg -l | grep -E 'rdma-core|libibverbs|ibverbs-providers|ibverbs-utils' || true"
    run_or_note "command status before" sh -c "command -v ibv_devices || true; command -v ibv_devinfo || true; command -v rdma || true"

    if fuser /var/lib/dpkg/lock-frontend /var/lib/dpkg/lock /var/cache/apt/archives/lock >/dev/null 2>&1; then
        printf '\nBLOCKED_DPKG_LOCK: dpkg/apt lock file is in use.\n'
        fuser -v /var/lib/dpkg/lock-frontend /var/lib/dpkg/lock /var/cache/apt/archives/lock || true
        printf 'Do not remove lock files while a process owns them. Retry after the package manager is free.\n'
        exit 0
    fi

    if pgrep -a unattended-upgr >/dev/null 2>&1; then
        printf '\nINFO unattended-upgrade process is visible, but no dpkg/apt lock holder was found.\n'
        pgrep -a unattended-upgr || true
        printf 'Proceeding because package locks are free.\n'
    fi

    run_or_note "apt-get update" sudo apt-get update
    run_or_note "apt-get install ibverbs-utils" sudo apt-get install -y ibverbs-utils
    run_or_note "dpkg package status after" sh -c "dpkg -l | grep -E 'rdma-core|libibverbs|ibverbs-providers|ibverbs-utils' || true"
    run_or_note "command status after" sh -c "command -v ibv_devices || true; command -v ibv_devinfo || true; command -v rdma || true"
} 2>&1 | tee "${log}"

printf 'INSTALL_LOG=%s\n' "${log}"
