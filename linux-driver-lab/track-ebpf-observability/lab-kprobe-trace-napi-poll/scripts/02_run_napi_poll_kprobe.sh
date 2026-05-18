#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
RD="$(last_record_dir)"
OUT="${RD}/NAPI_POLL_KPROBE.log"
TMP_BT="${RD}/napi_poll_kprobe_dynamic.bt"

{
    echo "LAB=${LAB_NAME}"
    echo "DATE=$(date -Iseconds)"
    echo "本脚本会动态选择第一个可观测的 NAPI poll 符号。"
    echo
} > "${OUT}"

if ! command -v bpftrace >/dev/null 2>&1; then
    echo "BPFTRACE_NOT_FOUND=1" | tee -a "${OUT}"
    echo "RC=127" >> "${OUT}"
    echo "NAPI_POLL_KPROBE=${OUT}"
    exit 0
fi

poll_sym="$(first_available_kprobe napi_poll __napi_poll poll_one_napi napi_threaded_poll || true)"
if [[ -z "${poll_sym}" ]]; then
    echo "NO_NAPI_POLL_KPROBE_AVAILABLE=1" | tee -a "${OUT}"
    echo "RC=0" >> "${OUT}"
    echo "NAPI_POLL_KPROBE=${OUT}"
    exit 0
fi

# 这里按 probe 维度计数，避免 fallback 到不同符号时日志失去上下文。
cat > "${TMP_BT}" <<BT
kprobe:${poll_sym}
{
    @napi_poll_calls[probe] = count();
    @napi_poll_by_cpu[probe, cpu] = count();
    @napi_poll_by_comm[probe, comm] = count();
}

interval:s:1
{
    print(@napi_poll_calls);
    print(@napi_poll_by_cpu);
    print(@napi_poll_by_comm);
}
BT

{
    echo "## selected probe"
    echo "kprobe:${poll_sym}"
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
echo "NAPI_POLL_KPROBE=${OUT}"
