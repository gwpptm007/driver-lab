#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(ensure_record_dir)"
log="${record_dir}/RDMA_CAPABILITY.log"

{
    printf '# RDMA capability check\n'
    printf 'record_dir=%s\n' "${record_dir}"
    printf 'timestamp=%s\n' "$(date -Is)"

    printf '\n===== command availability =====\n'
    for cmd in ibv_devices ibv_devinfo rdma; do
        command_status "${cmd}"
    done

    if command -v ibv_devices >/dev/null 2>&1; then
        run_or_note "ibv_devices" ibv_devices
    else
        printf '\nSKIP ibv_devices: command missing\n'
    fi

    if command -v ibv_devinfo >/dev/null 2>&1; then
        run_or_note "ibv_devinfo" ibv_devinfo
        run_or_note "ibv_devinfo verbose" ibv_devinfo -v
    else
        printf '\nSKIP ibv_devinfo: command missing\n'
    fi

    if command -v rdma >/dev/null 2>&1; then
        run_or_note "rdma link" rdma link
        run_or_note "rdma dev" rdma dev
        run_or_note "rdma resource show" rdma resource show
    else
        printf '\nSKIP rdma: command missing\n'
    fi
} 2>&1 | tee "${log}"

printf 'RDMA_CAPABILITY_LOG=%s\n' "${log}"
