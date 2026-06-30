#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(ensure_record_dir)"
log="${record_dir}/SOFT_ROCE_BOUNDARY.log"

{
    printf '# Soft-RoCE boundary check\n'
    printf 'record_dir=%s\n' "${record_dir}"
    printf 'timestamp=%s\n' "$(date -Is)"
    printf 'ENABLE_RXE_SETUP=%s\n' "${ENABLE_RXE_SETUP:-0}"
    printf 'RXE_NETDEV=%s\n' "${RXE_NETDEV:-}"

    run_or_note "rdma_rxe modinfo" sh -c 'modinfo rdma_rxe 2>/dev/null || true'
    run_or_note "rdma modules loaded" sh -c "lsmod | grep -Ei 'rdma_rxe|ib_core|rdma_cm|rxe' || true"

    if [[ "${ENABLE_RXE_SETUP:-0}" != "1" ]]; then
        printf '\nINFO setup skipped. Set ENABLE_RXE_SETUP=1 RXE_NETDEV=<netdev> to try creating a Soft-RoCE device.\n'
        exit 0
    fi

    if [[ -z "${RXE_NETDEV:-}" ]]; then
        printf '\nERROR RXE_NETDEV is required when ENABLE_RXE_SETUP=1\n'
        exit 0
    fi

    if ! command -v rdma >/dev/null 2>&1; then
        printf '\nERROR rdma command missing; cannot create rxe link.\n'
        exit 0
    fi

    if ! ip link show "${RXE_NETDEV}" >/dev/null 2>&1; then
        printf '\nERROR netdev %s not found.\n' "${RXE_NETDEV}"
        exit 0
    fi

    run_or_note "modprobe rdma_rxe" sudo modprobe rdma_rxe
    run_or_note "rdma link before setup" rdma link
    run_or_note "rdma link add rxe0" sudo rdma link add rxe0 type rxe netdev "${RXE_NETDEV}"
    run_or_note "rdma link after setup" rdma link
    run_or_note "ibv_devices after setup" sh -c 'ibv_devices 2>/dev/null || true'
} 2>&1 | tee "${log}"

printf 'SOFT_ROCE_LOG=%s\n' "${log}"
