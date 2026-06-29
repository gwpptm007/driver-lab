#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
APP_DIR="${PROJECT_ROOT}/app"
TOOLS_DIR="${PROJECT_ROOT}/tools"
APP_BIN="${APP_DIR}/build/dpdk-l3-forwarder-lite"

: "${L3_LCORES:=0-1}"
: "${L3_MEMORY_CHANNELS:=4}"
: "${L3_FILE_PREFIX:=dpdk_l3_forwarder_lite}"
: "${L3_PCAP_COUNT:=48}"
: "${L3_BURST_SIZE:=16}"
: "${L3_MBUF_CACHE:=250}"
: "${L3_MAX_IDLE_POLLS:=100000}"

if [[ -z "${RECORD_DIR:-}" ]]; then
  RECORD_DIR="${PROJECT_ROOT}/records/$(date +%Y%m%d-%H%M%S)-l3-forwarder"
fi
mkdir -p "${RECORD_DIR}"
PCAP_FILE="${RECORD_DIR}/l3_input.pcap"

log_env() {
  cat <<EOF
PROJECT_ROOT=${PROJECT_ROOT}
APP_BIN=${APP_BIN}
RECORD_DIR=${RECORD_DIR}
PCAP_FILE=${PCAP_FILE}
L3_LCORES=${L3_LCORES}
L3_MEMORY_CHANNELS=${L3_MEMORY_CHANNELS}
L3_FILE_PREFIX=${L3_FILE_PREFIX}
L3_PCAP_COUNT=${L3_PCAP_COUNT}
L3_BURST_SIZE=${L3_BURST_SIZE}
L3_MBUF_CACHE=${L3_MBUF_CACHE}
L3_MAX_IDLE_POLLS=${L3_MAX_IDLE_POLLS}
EOF
}

require_bin() {
  [[ -x "${APP_BIN}" ]] || { echo "MISS app/build/dpdk-l3-forwarder-lite" >&2; exit 1; }
}

