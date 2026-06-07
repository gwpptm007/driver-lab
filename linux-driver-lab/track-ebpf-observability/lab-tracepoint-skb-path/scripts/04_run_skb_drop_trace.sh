#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/SKB_DROP_TRACE.log"
TMP_BT="${RD}/skb_drop_trace_dynamic.bt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo "本脚本使用 skb:kfree_skb tracepoint 观察 skb 释放事件。"
    echo "注意: 此内核上 kfree_skb 的 args->location (enum skb_drop_reason) BTF 不完整，只观测基本计数。"
    echo
} > "${OUT}"

if ! command -v bpftrace >/dev/null 2>&1; then
    echo "BPFTRACE_NOT_FOUND=1" | tee -a "${OUT}"
    echo "RC=127" >> "${OUT}"
    echo "SKB_DROP_TRACE=${OUT}"
    exit 0
fi

cat > "${TMP_BT}" <<'BT'
tracepoint:skb:kfree_skb
{
    @kfree_total = count();
    @kfree_by_cpu[cpu] = count();
    @kfree_by_comm[comm] = count();
}

interval:s:1
{
    printf("=== skb kfree_skb === @ %ds ===\n", elapsed);
    print(@kfree_total);
    print(@kfree_by_cpu);
    print(@kfree_by_comm);
}
BT

{
    echo "## tracepoints used"
    echo "tracepoint:skb:kfree_skb (basic: cpu, comm only)"
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
echo "SKB_DROP_TRACE=${OUT}"
