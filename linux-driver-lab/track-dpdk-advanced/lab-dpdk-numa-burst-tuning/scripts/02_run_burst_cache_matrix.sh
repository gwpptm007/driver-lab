#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_bin

python3 "${TOOLS_DIR}/gen_udp_pcap.py" "${PCAP_FILE}" "${TUNE_PCAP_COUNT}" | tee "${RECORD_DIR}/PCAP_GENERATE.log"
CSV="${RECORD_DIR}/MATRIX.csv"
LOG="${RECORD_DIR}/MATRIX.log"
echo "burst_size,mbuf_cache,rx_packets,rx_bytes,duration_sec,pps,polls,empty_polls" > "$CSV"
: > "$LOG"

run_id=0
for burst in ${TUNE_BURST_LIST}; do
  for cache in ${TUNE_CACHE_LIST}; do
    run_id=$((run_id + 1))
    prefix="${TUNE_FILE_PREFIX}_${run_id}_${burst}_${cache}"
    echo "## RUN burst=${burst} cache=${cache}" | tee -a "$LOG"
    out=$("${APP_BIN}" \
      -l "${TUNE_LCORES}" \
      -n "${TUNE_MEMORY_CHANNELS}" \
      --file-prefix "${prefix}" \
      --vdev "net_pcap0,rx_pcap=${PCAP_FILE}" \
      -- \
      --burst-size "${burst}" \
      --mbuf-cache "${cache}" \
      --nb-mbuf "${TUNE_NB_MBUF}" \
      --rx-desc "${TUNE_RX_DESC}" 2>&1)
    echo "$out" | tee -a "$LOG"
    result=$(echo "$out" | grep '^RESULT ' | tail -1)
    rx=$(echo "$result" | tr ' ' '\n' | awk -F= '$1=="rx_packets"{print $2}')
    bytes=$(echo "$result" | tr ' ' '\n' | awk -F= '$1=="rx_bytes"{print $2}')
    dur=$(echo "$result" | tr ' ' '\n' | awk -F= '$1=="duration_sec"{print $2}')
    pps=$(echo "$result" | tr ' ' '\n' | awk -F= '$1=="pps"{print $2}')
    polls=$(echo "$result" | tr ' ' '\n' | awk -F= '$1=="polls"{print $2}')
    empty=$(echo "$result" | tr ' ' '\n' | awk -F= '$1=="empty_polls"{print $2}')
    echo "${burst},${cache},${rx},${bytes},${dur},${pps},${polls},${empty}" >> "$CSV"
  done
done

echo "[OK] matrix saved: $CSV"
