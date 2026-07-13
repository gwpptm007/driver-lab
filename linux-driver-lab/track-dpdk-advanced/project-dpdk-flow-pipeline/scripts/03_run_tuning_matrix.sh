#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

: "${FLOW_TUNING_PCAP_COUNT:=4096}"
: "${FLOW_TUNING_IDLE_POLLS:=10000}"
TUNING_DIR="${FLOW_RECORD_DIR}/tuning"
mkdir -p "${TUNING_DIR}"
TUNING_PCAP="${TUNING_DIR}/tuning_input.pcap"

[[ -x "${APP_BIN}" ]] || { echo "missing app: ${APP_BIN}" >&2; exit 1; }
python3 "${TOOLS_DIR}/gen_flow_pcap.py" "${TUNING_PCAP}" "${FLOW_TUNING_PCAP_COUNT}"

# 每次只改变一个主变量；rule case 保持 burst/cache 与 baseline 一致。
while read -r name burst cache extra_rules; do
  log="${TUNING_DIR}/${name}.log"
  "${APP_BIN}" \
    -l "${FLOW_LCORES}" -n "${FLOW_MEMORY_CHANNELS}" --no-pci --no-huge \
    --file-prefix "${FLOW_FILE_PREFIX}_${name}_$$" \
    --vdev "net_pcap0,rx_pcap=${TUNING_PCAP}" \
    --vdev "net_null1" \
    -- \
    --burst-size "${burst}" \
    --mbuf-cache "${cache}" \
    --expected-packets "${FLOW_TUNING_PCAP_COUNT}" \
    --extra-rules "${extra_rules}" \
    --max-idle-polls "${FLOW_TUNING_IDLE_POLLS}" >"${log}" 2>&1
  grep -q 'FLOW_RESULT hash_hits=3072 hash_misses=1024 rule_drop=1024 forward=1024 mark=1024 default_drop=1024 invalid=0' "${log}"
  grep -q 'FLOW_LATENCY samples=4096 ' "${log}"
  grep -q 'DPDK_FLOW_PIPELINE_PHASE3_WORKER_PASS' "${log}"
  grep -q 'cleanup=complete result=pass' "${log}"
  echo "FLOW_TUNING_CASE_PASS name=${name} burst=${burst} cache=${cache} extra_rules=${extra_rules}"
done <<'EOF'
baseline 16 250 0
burst_1 1 250 0
burst_32 32 250 0
burst_64 64 250 0
cache_0 16 0 0
rules_64 16 250 61
rules_512 16 250 509
EOF

python3 "${TOOLS_DIR}/parse_tuning_matrix.py" "${TUNING_DIR}"
cat "${TUNING_DIR}/TUNING_MATRIX.md"
echo 'DPDK_FLOW_PIPELINE_PHASE5_TUNING_PASS'
