#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(ensure_record_dir)"
log="${record_dir}/ENV_CHECK.log"

{
    printf '# RDMA env check\n'
    printf 'record_dir=%s\n' "${record_dir}"
    printf 'timestamp=%s\n' "$(date -Is)"

    run_or_note "whoami" whoami
    run_or_note "hostname" hostname
    run_or_note "uname" uname -a
    run_or_note "os-release" sh -c 'cat /etc/os-release 2>/dev/null || true'
    run_or_note "kernel cmdline" sh -c 'cat /proc/cmdline 2>/dev/null || true'
    run_or_note "cpu summary" lscpu
    run_or_note "pci ethernet/infiniband" sh -c "lspci -nn 2>/dev/null | grep -Ei 'ethernet|network|infiniband|mellanox|broadcom|intel|chelsio|qlogic' || true"
    run_or_note "ip brief link" ip -br link
    run_or_note "ip brief addr" ip -br addr
    run_or_note "loaded rdma modules" sh -c "lsmod | grep -Ei '(^ib_|rdma|mlx|rxe|iw_cxgb|irdma|bnxt_re)' || true"
    run_or_note "rdma_rxe modinfo" sh -c 'modinfo rdma_rxe 2>/dev/null || true'

    printf '\n===== command availability =====\n'
    for cmd in ibv_devices ibv_devinfo rdma rxe_cfg ethtool lspci modinfo lsmod; do
        command_status "${cmd}"
    done
} 2>&1 | tee "${log}"

printf 'ENV_LOG=%s\n' "${log}"
