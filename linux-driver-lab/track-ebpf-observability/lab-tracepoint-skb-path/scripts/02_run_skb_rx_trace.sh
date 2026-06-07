#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/SKB_RX_TRACE.log"
TMP_BT="${RD}/skb_rx_trace_dynamic.bt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo "本脚本使用稳定 tracepoint 观察 skb 收包路径（netif_receive_skb + napi_gro_receive_entry）。"
    echo
} > "${OUT}"

if ! command -v bpftrace >/dev/null 2>&1; then
    echo "BPFTRACE_NOT_FOUND=1" | tee -a "${OUT}"
    echo "RC=127" >> "${OUT}"
    echo "SKB_RX_TRACE=${OUT}"
    exit 0
fi

cat > "${TMP_BT}" <<'BT'
tracepoint:net:netif_receive_skb
{
    @rx_total = count();
    @rx_by_cpu[cpu] = count();
    @rx_by_name[args->name] = count();
}

tracepoint:net:napi_gro_receive_entry
{
    @gro_entry_total = count();
    @gro_by_cpu[cpu] = count();
    @gro_by_name[args->name] = count();
}

interval:s:1
{
    printf("=== skb RX tracepoint === @ %ds ===\n", elapsed);
    print(@rx_total);
    print(@rx_by_cpu);
    print(@rx_by_name);
    print(@gro_entry_total);
    print(@gro_by_cpu);
    print(@gro_by_name);
}
BT

{
    echo "## tracepoints used"
    echo "tracepoint:net:netif_receive_skb"
    echo "tracepoint:net:napi_gro_receive_entry"
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
echo "SKB_RX_TRACE=${OUT}"
