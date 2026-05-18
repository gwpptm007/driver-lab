#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/SOFTIRQ_NAPI_CORRELATION.log"
TMP_BT="${RD}/softirq_napi_correlation_dynamic.bt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo "本脚本始终观测 NET_RX softirq；如果存在可挂载的 NAPI poll 符号，则一起关联观测。"
    echo
} > "${OUT}"

if ! command -v bpftrace >/dev/null 2>&1; then
    echo "BPFTRACE_NOT_FOUND=1" | tee -a "${OUT}"
    echo "RC=127" >> "${OUT}"
    echo "SOFTIRQ_NAPI_CORRELATION=${OUT}"
    exit 0
fi

poll_sym="$(first_available_kprobe napi_poll __napi_poll poll_one_napi napi_threaded_poll || true)"

# softirq tracepoint 是稳定入口，NAPI kprobe 是增强证据，两者解耦避免单点失败。
cat > "${TMP_BT}" <<'BT'
tracepoint:irq:softirq_entry
/args->vec == 3/
{
    @net_rx_softirq_entry = count();
    @net_rx_softirq_entry_cpu[cpu] = count();
}

tracepoint:irq:softirq_exit
/args->vec == 3/
{
    @net_rx_softirq_exit = count();
    @net_rx_softirq_exit_cpu[cpu] = count();
}
BT

if [[ -n "${poll_sym}" ]]; then
    # 只有确认符号存在时才生成 kprobe 段，避免 attach 失败拖垮 softirq 观测。
    cat >> "${TMP_BT}" <<BT

kprobe:${poll_sym}
{
    @napi_poll_calls[probe] = count();
    @napi_poll_cpu[probe, cpu] = count();
}
BT
fi

cat >> "${TMP_BT}" <<'BT'

interval:s:1
{
    print(@net_rx_softirq_entry);
    print(@net_rx_softirq_entry_cpu);
    print(@net_rx_softirq_exit);
    print(@net_rx_softirq_exit_cpu);
BT

if [[ -n "${poll_sym}" ]]; then
    cat >> "${TMP_BT}" <<'BT'
    print(@napi_poll_calls);
    print(@napi_poll_cpu);
BT
fi

cat >> "${TMP_BT}" <<'BT'
}
BT

{
    echo "## selected probes"
    echo "tracepoint:irq:softirq_entry"
    echo "tracepoint:irq:softirq_exit"
    if [[ -n "${poll_sym}" ]]; then
        echo "kprobe:${poll_sym}"
    else
        echo "NO_NAPI_POLL_KPROBE_AVAILABLE=1"
    fi
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
echo "SOFTIRQ_NAPI_CORRELATION=${OUT}"
