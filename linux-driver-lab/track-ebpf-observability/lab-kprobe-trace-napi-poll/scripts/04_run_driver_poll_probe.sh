#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/DRIVER_POLL_OPTIONAL.log"
TMP_BT="${RD}/driver_poll_dynamic.bt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo "本脚本是补充观测：动态选择可用的驱动 poll 或 NAPI helper 符号。"
    echo
} > "${OUT}"

if ! command -v bpftrace >/dev/null 2>&1; then
    echo "BPFTRACE_NOT_FOUND=1" | tee -a "${OUT}"
    echo "RC=127" >> "${OUT}"
    exit 0
fi

# 候选符号覆盖常见网卡驱动和通用 NAPI helper；不存在的符号会自动跳过。
mapfile -t avail < <(
    for sym in vmxnet3_poll mlx5e_napi_poll ixgbe_poll i40e_napi_poll ena_io_poll napi_complete_done napi_gro_receive; do
        if kprobe_exists "${sym}"; then
            echo "kprobe:${sym}"
        fi
    done
)

if [[ ${#avail[@]} -eq 0 ]]; then
    echo "NO_DRIVER_POLL_PROBES_AVAILABLE=1" | tee -a "${OUT}"
    echo "RC=0" >> "${OUT}"
    exit 0
fi

# 多个 kprobe 共用一个动作块，用 probe 变量区分来源。
{
    printf '%s' "${avail[0]}"
    for ((i=1; i<${#avail[@]}; i++)); do
        printf ',\n%s' "${avail[$i]}"
    done
    cat <<'BT'
{
    @driver_or_napi_helpers[probe] = count();
    @driver_or_napi_cpu[probe, cpu] = count();
}

interval:s:1
{
    print(@driver_or_napi_helpers);
    print(@driver_or_napi_cpu);
}
BT
} > "${TMP_BT}"

{
    echo "## selected probes"
    printf '%s\n' "${avail[@]}"
    echo
    echo "## generated script"
    cat "${TMP_BT}"
    echo
} >> "${OUT}"

set +e
timeout "${EBPF_DURATION}" bpftrace "${TMP_BT}" >> "${OUT}" 2>&1
rc=$?
set -e
echo "RC=${rc}" >> "${OUT}"
if [[ "${rc}" == "124" ]]; then
    echo "TIMEOUT_AS_EXPECTED=1" >> "${OUT}"
fi

echo "DRIVER_POLL_OPTIONAL=${OUT}"
