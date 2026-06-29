#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
APP_DIR="${PROJECT_ROOT}/app"
TOOLS_DIR="${PROJECT_ROOT}/tools"
APP_BIN="${APP_DIR}/build/dpdk-rss-queue-probe"

: "${RSS_LCORES:=0-3}"
: "${RSS_MEMORY_CHANNELS:=4}"
: "${RSS_FILE_PREFIX:=dpdk_rss_queue_probe}"
: "${RSS_REQUEST_RX_QUEUES:=2}"
: "${RSS_REQUEST_TX_QUEUES:=0}"
: "${RSS_NB_MBUF:=8192}"
: "${RSS_MBUF_CACHE:=250}"
: "${RSS_RX_DESC:=512}"
: "${RSS_TX_DESC:=512}"
: "${RSS_PCAP_COUNT:=64}"

if [[ -z "${RECORD_DIR:-}" ]]; then
  RECORD_DIR="${PROJECT_ROOT}/records/$(date +%Y%m%d-%H%M%S)-rss-multiqueue"
fi
mkdir -p "${RECORD_DIR}"

PCAP_FILE="${RECORD_DIR}/rss_input.pcap"

log_env() {
  cat <<EOF
PROJECT_ROOT=${PROJECT_ROOT}
APP_DIR=${APP_DIR}
APP_BIN=${APP_BIN}
RECORD_DIR=${RECORD_DIR}
PCAP_FILE=${PCAP_FILE}
RSS_LCORES=${RSS_LCORES}
RSS_MEMORY_CHANNELS=${RSS_MEMORY_CHANNELS}
RSS_FILE_PREFIX=${RSS_FILE_PREFIX}
RSS_REQUEST_RX_QUEUES=${RSS_REQUEST_RX_QUEUES}
RSS_REQUEST_TX_QUEUES=${RSS_REQUEST_TX_QUEUES}
RSS_NB_MBUF=${RSS_NB_MBUF}
RSS_MBUF_CACHE=${RSS_MBUF_CACHE}
RSS_RX_DESC=${RSS_RX_DESC}
RSS_TX_DESC=${RSS_TX_DESC}
RSS_PCAP_COUNT=${RSS_PCAP_COUNT}
EOF
}

require_bin() {
  if [[ ! -x "${APP_BIN}" ]]; then
    echo "MISS app/build/dpdk-rss-queue-probe" >&2
    echo "Run: ./scripts/01_build.sh" >&2
    exit 1
  fi
}
