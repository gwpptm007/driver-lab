#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"
require_bin

OUT="${RECORD_DIR}/PCAP_QUEUE_PROBE.log"

python3 "${TOOLS_DIR}/gen_udp_pcap.py" "${PCAP_FILE}" "${RSS_PCAP_COUNT}" | tee "${RECORD_DIR}/PCAP_GENERATE.log"

{
  echo "# PCAP_QUEUE_PROBE"
  echo
  log_env
  echo
  echo "## command"
  printf '%q ' \
    "${APP_BIN}" \
    -l "${RSS_LCORES}" \
    -n "${RSS_MEMORY_CHANNELS}" \
    --file-prefix "${RSS_FILE_PREFIX}" \
    --vdev "net_pcap0,rx_pcap=${PCAP_FILE}" \
    -- \
    --rx-queues "${RSS_REQUEST_RX_QUEUES}" \
    --tx-queues "${RSS_REQUEST_TX_QUEUES}" \
    --rx-desc "${RSS_RX_DESC}" \
    --tx-desc "${RSS_TX_DESC}" \
    --nb-mbuf "${RSS_NB_MBUF}" \
    --mbuf-cache "${RSS_MBUF_CACHE}" \
    --enable-rss 1
  echo
  echo
  "${APP_BIN}" \
    -l "${RSS_LCORES}" \
    -n "${RSS_MEMORY_CHANNELS}" \
    --file-prefix "${RSS_FILE_PREFIX}" \
    --vdev "net_pcap0,rx_pcap=${PCAP_FILE}" \
    -- \
    --rx-queues "${RSS_REQUEST_RX_QUEUES}" \
    --tx-queues "${RSS_REQUEST_TX_QUEUES}" \
    --rx-desc "${RSS_RX_DESC}" \
    --tx-desc "${RSS_TX_DESC}" \
    --nb-mbuf "${RSS_NB_MBUF}" \
    --mbuf-cache "${RSS_MBUF_CACHE}" \
    --enable-rss 1
} 2>&1 | tee "${OUT}"

echo "[OK] queue probe saved: ${OUT}"
