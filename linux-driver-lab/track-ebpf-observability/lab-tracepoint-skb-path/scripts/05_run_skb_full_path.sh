#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/SKB_FULL_PATH.log"
TMP_BT="${RD}/skb_full_path_dynamic.bt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo "本脚本将 RX + TX tracepoint 合并到一次观测中，实现全路径关联。"
    echo "注意: skb:kfree_skb 在此内核上 BTF 不完整，本次不包含，由 04 独立观测。"
    echo
} > "${OUT}"

if ! command -v bpftrace >/dev/null 2>&1; then
    echo "BPFTRACE_NOT_FOUND=1" | tee -a "${OUT}"
    echo "RC=127" >> "${OUT}"
    echo "SKB_FULL_PATH=${OUT}"
    exit 0
fi

cat > "${TMP_BT}" <<'BT'
tracepoint:net:netif_receive_skb
{
    @rx = count();
    @rx_cpu[cpu] = count();
    @rx_dev[args->name] = count();
}

tracepoint:net:napi_gro_receive_entry
{
    @gro = count();
}

tracepoint:net:net_dev_queue
{
    @txq = count();
    @txq_cpu[cpu] = count();
    @txq_dev[args->name] = count();
}

tracepoint:net:net_dev_start_xmit
{
    @tx = count();
    @tx_dev[args->name] = count();
    @tx_len = hist(args->len);
}

interval:s:1
{
    printf("\n=== skb full-path === @ %ds ===\n", elapsed);
    print(@rx);
    print(@rx_cpu);
    print(@rx_dev);
    print(@gro);
    print(@txq);
    print(@txq_cpu);
    print(@txq_dev);
    print(@tx);
    print(@tx_dev);
    print(@tx_len);
}
BT

{
    echo "## tracepoints used"
    echo "tracepoint:net:netif_receive_skb"
    echo "tracepoint:net:napi_gro_receive_entry"
    echo "tracepoint:net:net_dev_queue"
    echo "tracepoint:net:net_dev_start_xmit"
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
echo "SKB_FULL_PATH=${OUT}"
