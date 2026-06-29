#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
APP_DIR="${PROJECT_ROOT}/app"
TOOLS_DIR="${PROJECT_ROOT}/tools"
APP_BIN="${APP_DIR}/build/dpdk-mbuf-inspect"

: "${MBUF_LCORES:=0-1}"
: "${MBUF_MEMORY_CHANNELS:=4}"
: "${MBUF_FILE_PREFIX:=dpdk_mbuf_inspect}"
: "${MBUF_RUN_SECONDS:=5}"
: "${MBUF_SAMPLE_LIMIT:=8}"
: "${MBUF_BURST_SIZE:=32}"
: "${MBUF_NB_MBUF:=8192}"
: "${MBUF_CACHE:=250}"
: "${MBUF_RX_DESC:=1024}"
: "${MBUF_PCAP_COUNT:=64}"

if [[ -z "${RECORD_DIR:-}" ]]; then
  RECORD_DIR="${PROJECT_ROOT}/records/$(date +%Y%m%d-%H%M%S)-mbuf-mempool"
fi
mkdir -p "${RECORD_DIR}"

PCAP_FILE="${RECORD_DIR}/udp_input.pcap"

log_env() {
  cat <<EOF
PROJECT_ROOT=${PROJECT_ROOT}
APP_DIR=${APP_DIR}
TOOLS_DIR=${TOOLS_DIR}
APP_BIN=${APP_BIN}
RECORD_DIR=${RECORD_DIR}
PCAP_FILE=${PCAP_FILE}
MBUF_LCORES=${MBUF_LCORES}
MBUF_MEMORY_CHANNELS=${MBUF_MEMORY_CHANNELS}
MBUF_FILE_PREFIX=${MBUF_FILE_PREFIX}
MBUF_RUN_SECONDS=${MBUF_RUN_SECONDS}
MBUF_SAMPLE_LIMIT=${MBUF_SAMPLE_LIMIT}
MBUF_BURST_SIZE=${MBUF_BURST_SIZE}
MBUF_NB_MBUF=${MBUF_NB_MBUF}
MBUF_CACHE=${MBUF_CACHE}
MBUF_RX_DESC=${MBUF_RX_DESC}
MBUF_PCAP_COUNT=${MBUF_PCAP_COUNT}
EOF
}

require_bin() {
  if [[ ! -x "${APP_BIN}" ]]; then
    echo "MISS app/build/dpdk-mbuf-inspect" >&2
    echo "Run: ./scripts/01_build.sh" >&2
    exit 1
  fi
}
