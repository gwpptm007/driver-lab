#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
require_bin

OUT="${RECORD_DIR}/PCAP_METADATA.log"

python3 "${TOOLS_DIR}/gen_udp_pcap.py" "${PCAP_FILE}" "${MBUF_PCAP_COUNT}" | tee "${RECORD_DIR}/PCAP_GENERATE.log"

{
  echo "# PCAP_METADATA"
  echo
  log_env
  echo
  echo "## command"
  printf '%q ' \
    "${APP_BIN}" \
    -l "${MBUF_LCORES}" \
    -n "${MBUF_MEMORY_CHANNELS}" \
    --file-prefix "${MBUF_FILE_PREFIX}" \
    --vdev "net_pcap0,rx_pcap=${PCAP_FILE}" \
    -- \
    --run-seconds "${MBUF_RUN_SECONDS}" \
    --sample-limit "${MBUF_SAMPLE_LIMIT}" \
    --burst-size "${MBUF_BURST_SIZE}" \
    --nb-mbuf "${MBUF_NB_MBUF}" \
    --mbuf-cache "${MBUF_CACHE}" \
    --rx-desc "${MBUF_RX_DESC}" \
    --promisc 1
  echo
  echo
  "${APP_BIN}" \
    -l "${MBUF_LCORES}" \
    -n "${MBUF_MEMORY_CHANNELS}" \
    --file-prefix "${MBUF_FILE_PREFIX}" \
    --vdev "net_pcap0,rx_pcap=${PCAP_FILE}" \
    -- \
    --run-seconds "${MBUF_RUN_SECONDS}" \
    --sample-limit "${MBUF_SAMPLE_LIMIT}" \
    --burst-size "${MBUF_BURST_SIZE}" \
    --nb-mbuf "${MBUF_NB_MBUF}" \
    --mbuf-cache "${MBUF_CACHE}" \
    --rx-desc "${MBUF_RX_DESC}" \
    --promisc 1
} 2>&1 | tee "${OUT}"

echo "[OK] pcap metadata run saved: ${OUT}"
