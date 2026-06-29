#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
APP_DIR="${PROJECT_ROOT}/app"
TOOLS_DIR="${PROJECT_ROOT}/tools"
APP_BIN="${APP_DIR}/build/dpdk-burst-cache-probe"

: "${TUNE_LCORES:=0-1}"
: "${TUNE_MEMORY_CHANNELS:=4}"
: "${TUNE_FILE_PREFIX:=dpdk_burst_cache_probe}"
: "${TUNE_NB_MBUF:=16384}"
: "${TUNE_RX_DESC:=1024}"
: "${TUNE_PCAP_COUNT:=4096}"
: "${TUNE_BURST_LIST:=1 4 16 32 64}"
: "${TUNE_CACHE_LIST:=0 64 250}"

if [[ -z "${RECORD_DIR:-}" ]]; then
  RECORD_DIR="${PROJECT_ROOT}/records/$(date +%Y%m%d-%H%M%S)-numa-burst"
fi
mkdir -p "${RECORD_DIR}"
PCAP_FILE="${RECORD_DIR}/burst_input.pcap"

log_env() {
  cat <<EOF
PROJECT_ROOT=${PROJECT_ROOT}
APP_BIN=${APP_BIN}
RECORD_DIR=${RECORD_DIR}
PCAP_FILE=${PCAP_FILE}
TUNE_LCORES=${TUNE_LCORES}
TUNE_MEMORY_CHANNELS=${TUNE_MEMORY_CHANNELS}
TUNE_FILE_PREFIX=${TUNE_FILE_PREFIX}
TUNE_NB_MBUF=${TUNE_NB_MBUF}
TUNE_RX_DESC=${TUNE_RX_DESC}
TUNE_PCAP_COUNT=${TUNE_PCAP_COUNT}
TUNE_BURST_LIST=${TUNE_BURST_LIST}
TUNE_CACHE_LIST=${TUNE_CACHE_LIST}
EOF
}
require_bin() { [[ -x "${APP_BIN}" ]] || { echo "MISS app/build/dpdk-burst-cache-probe" >&2; exit 1; }; }
