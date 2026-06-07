#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/SKB_TX_TRACE.log"
TMP_BT="${RD}/skb_tx_trace_dynamic.bt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo "本脚本使用稳定 tracepoint 观察 skb 发包路径（net_dev_queue + net_dev_start_xmit）。"
    echo
} > "${OUT}"

if ! command -v bpftrace >/dev/null 2>&1; then
    echo "BPFTRACE_NOT_FOUND=1" | tee -a "${OUT}"
    echo "RC=127" >> "${OUT}"
    echo "SKB_TX_TRACE=${OUT}"
    exit 0
fi

cat > "${TMP_BT}" <<'BT'
tracepoint:net:net_dev_queue
{
    @tx_queue_total = count();
    @tx_queue_by_cpu[cpu] = count();
    @tx_queue_by_name[args->name] = count();
}

tracepoint:net:net_dev_start_xmit
{
    @tx_xmit_total = count();
    @tx_xmit_by_cpu[cpu] = count();
    @tx_xmit_by_name[args->name] = count();
    @tx_xmit_len = hist(args->len);
}

interval:s:1
{
    printf("=== skb TX tracepoint === @ %ds ===\n", elapsed);
    print(@tx_queue_total);
    print(@tx_queue_by_cpu);
    print(@tx_queue_by_name);
    print(@tx_xmit_total);
    print(@tx_xmit_by_cpu);
    print(@tx_xmit_by_name);
    print(@tx_xmit_len);
}
BT

{
    echo "## tracepoints used"
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
echo "SKB_TX_TRACE=${OUT}"
