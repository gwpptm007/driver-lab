#!/usr/bin/env bash
#===============================================================================
# 06_run_pcap_rx_test.sh - pcap PMD 确定性 UDP 功能回归
#
# Topology:
#   net_pcap0 (rx from pcap, infinite replay) -> fastpath-lite -> net_null0 (tx discard)
#
# fastpath-lite port pairing: port 0 (pcap) <-> port 1 (null)
#   - port 0 RX from pcap, classify, forward to port 1
#   - port 1 TX to null (accepts & discards)
#
# 历史 parser marker 保持兼容；按 track 证据口径应解释为：
#   PASS_PCAP_FUNCTIONAL: rx>0, ipv4>0, udp>0
#   PASS_PCAP_FORWARDING: tx>0 (port 1 tx to null)
#   PASS_PCAP_REWRITE: rewrite>0 (when --rewrite flag is set)
#===============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FASTPATH_BIN="${PROJECT_ROOT}/../project-user-space-fastpath/app/build/fastpath-lite"
TOOLS_DIR="${PROJECT_ROOT}/tools"

PCAP_FILE="${PCAP_FILE:-/tmp/udp_test.pcap}"
PCAP_PKT_COUNT="${PCAP_PKT_COUNT:-500}"
RUN_SECONDS="${RUN_SECONDS:-10}"
STATS_PERIOD="${STATS_PERIOD:-2}"
BURST_SIZE="${BURST_SIZE:-32}"
FILE_PREFIX="${FILE_PREFIX:-fastpath_pcap_test}"
UDP_ONLY="${UDP_ONLY:-0}"
SWAP_MAC="${SWAP_MAC:-1}"
REWRITE_ENABLE="${REWRITE_ENABLE:-0}"
REWRITE_DST_MAC="${REWRITE_DST_MAC:-52:54:00:00:00:02}"
REWRITE_DST_IP="${REWRITE_DST_IP:-10.10.20.20}"
REWRITE_DST_PORT="${REWRITE_DST_PORT:-10000}"

RECORD_TAG="${RECORD_TAG:-$(date +%Y%m%d_%H%M%S)-fastpath-pcap}"
RECORD_DIR="${RECORD_DIR:-${PROJECT_ROOT}/records/${RECORD_TAG}}"
mkdir -p "${RECORD_DIR}"

OUT="${RECORD_DIR}/FASTPATH_PCAP_RX.log"
CMD_OUT="${RECORD_DIR}/FASTPATH_PCAP_RX_COMMAND.txt"
STATS_OUT="${RECORD_DIR}/FASTPATH_PCAP_RX_STATS.txt"

# ----------------------------------
# Check prerequisites
# ----------------------------------
if [[ ! -x "${FASTPATH_BIN}" ]]; then
  echo "[ERR] fastpath binary not found: ${FASTPATH_BIN}" >&2
  echo "Run: cd ../project-user-space-fastpath && ./scripts/01_build_app.sh" >&2
  exit 1
fi

# ----------------------------------
# Generate pcap file if it doesn't exist
# ----------------------------------
if [[ ! -f "${PCAP_FILE}" ]]; then
  echo "[GEN] generating pcap file: ${PCAP_FILE} (${PCAP_PKT_COUNT} packets)"
  python3 "${TOOLS_DIR}/gen_udp_pcap.py" "${PCAP_FILE}" "${PCAP_PKT_COUNT}"
fi

# ----------------------------------
# Build command
# ----------------------------------
# net_pcap0: reads packets from pcap file (infinite loop replay)
# net_null0: discards all TX packets (always succeeds)
#
# With port pairing (0<->1):
#   - Packets from pcap arrive on port 0 -> classify -> forward to port 1
#   - Port 1 (net_null) accepts and discards, counted as TX success

CMD=("${FASTPATH_BIN}"
  -l 0-1
  -n 4
  --no-huge
  --file-prefix "${FILE_PREFIX}"
  --no-pci
  --vdev "net_pcap0,rx_pcap=${PCAP_FILE},infinite_rx=1"
  --vdev net_null0
  --
  --run-seconds "${RUN_SECONDS}"
  --stats-period "${STATS_PERIOD}"
  --burst-size "${BURST_SIZE}"
  --promisc 1
  --udp-only "${UDP_ONLY}"
  --swap-mac "${SWAP_MAC}"
  --rewrite "${REWRITE_ENABLE}"
)

# Append rewrite targets if rewrite is enabled
if [[ "${REWRITE_ENABLE}" == "1" ]]; then
  CMD+=(
    --rewrite-dst-mac "${REWRITE_DST_MAC}"
    --rewrite-dst-ip "${REWRITE_DST_IP}"
    --rewrite-dst-port "${REWRITE_DST_PORT}"
  )
fi

# Print and save command
{
  echo "# FASTPATH_PCAP_RX"
  echo
  echo "PROJECT_ROOT=${PROJECT_ROOT}"
  echo "FASTPATH_BIN=${FASTPATH_BIN}"
  echo "RECORD_DIR=${RECORD_DIR}"
  echo "PCAP_FILE=${PCAP_FILE}"
  echo "PCAP_PKT_COUNT=${PCAP_PKT_COUNT}"
  echo "RUN_SECONDS=${RUN_SECONDS}"
  echo "REWRITE_ENABLE=${REWRITE_ENABLE}"
  echo
  echo "## command"
  printf '%q ' "${CMD[@]}"
  echo
  echo
  echo "## note"
  echo "pcap PMD test: no external traffic source needed."
  echo "net_pcap0 replays UDP packets from pcap file in infinite loop."
  echo
} | tee "${CMD_OUT}"

# Run fastpath-lite
"${CMD[@]}" 2>&1 | tee "${OUT}"
rc=${PIPESTATUS[0]}

# ----------------------------------
# Parse stats
# ----------------------------------
if [[ -f "${TOOLS_DIR}/parse_fastpath_stats.py" ]]; then
  echo "[STATS] parsing output..."
  python3 "${TOOLS_DIR}/parse_fastpath_stats.py" "${OUT}" | tee "${STATS_OUT}"
fi

echo
echo "[OK] pcap rx test saved: ${OUT}"
echo "[OK] stats: ${STATS_OUT}"
echo "[OK] exit code: ${rc}"
