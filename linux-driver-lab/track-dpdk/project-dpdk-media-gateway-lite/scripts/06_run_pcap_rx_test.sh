#!/usr/bin/env bash
#===============================================================================
# 06_run_pcap_rx_test.sh - pcap PMD 确定性 UDP 功能回归
#
# Topology:
#   net_pcap0 (rx from pcap) -> media-gateway-lite -> net_null0 (tx discard)
#
# 历史 parser marker 保持兼容；按 track 证据口径应解释为：
#   PASS_PCAP_FUNCTIONAL: rx>0, ipv4>0, udp>0
#   PASS_PCAP_FORWARDING: tx>0, rule_hit>0  (via auto bidirectional rules)
#
# To also get PASS_PCAP_REWRITE, the script supports optional rewrite rules
# which adds explicit rules with dst_port matching and IP/MAC/Port rewrite.
#===============================================================================
set -euo pipefail

source "$(dirname "$0")/common.sh"
require_media_bin

PCAP_FILE="${PCAP_FILE:-/tmp/udp_test.pcap}"
PCAP_PKT_COUNT="${PCAP_PKT_COUNT:-500}"
APPEND_EXTRA="${APPEND_EXTRA:-}"  # extra args appended after the command line

OUT="${RECORD_DIR}/MEDIA_GATEWAY_PCAP_RX_TEST.log"
CMD_OUT="${RECORD_DIR}/MEDIA_GATEWAY_PCAP_RX_TEST_COMMAND.txt"
STATS_OUT="${RECORD_DIR}/MEDIA_GATEWAY_PCAP_RX_TEST_STATS.txt"

# ----------------------------------
# Generate pcap file if it doesn't exist
# ----------------------------------
TOOLS_DIR="${PROJECT_ROOT}/tools"
if [[ ! -f "${PCAP_FILE}" ]]; then
  echo "[GEN] generating pcap file: ${PCAP_FILE} (${PCAP_PKT_COUNT} packets)"
  python3 "${TOOLS_DIR}/gen_udp_pcap.py" "${PCAP_FILE}" "${PCAP_PKT_COUNT}"
fi

# ----------------------------------
# Build command
# ----------------------------------
# net_pcap0:  reads packets from pcap file (infinite loop replay)
# net_null0:   discards all TX packets (always succeeds)
#
# With auto bidirectional rules (0->1, 1->0):
#   - Packets from pcap arrive on port 0, match rule by in_port, forward to port 1
#   - Port 1 (net_null) accepts and discards, counted as TX success
CMD=("${MEDIA_BIN}"
  -l "${MEDIA_LCORES}"
  -n "${MEDIA_MEMORY_CHANNELS}"
  --no-huge
  --file-prefix "${MEDIA_FILE_PREFIX}_pcap_rx"
  --no-pci
  --vdev "net_pcap0,rx_pcap=${PCAP_FILE},infinite_rx=1"
  --vdev net_null0
  --
  --run-seconds "${MEDIA_RUN_SECONDS}"
  --stats-period "${MEDIA_STATS_PERIOD}"
  --burst-size "${MEDIA_BURST_SIZE}"
  --promisc "${MEDIA_PROMISC}"
  --udp-only "${MEDIA_UDP_ONLY}"
  --swap-mac "${MEDIA_SWAP_MAC}"
  --strict-rules "${MEDIA_STRICT_RULES}"
)

# Optionally append explicit rules for rewrite testing
if [[ "${MEDIA_EXTRA_APP_ARGS:-}" != "" ]]; then
  # parse extra args by splitting on spaces
  for extra in ${MEDIA_EXTRA_APP_ARGS}; do
    CMD+=("${extra}")
  done
fi

# print command and execute
printf '%q ' "${CMD[@]}" | tee "${CMD_OUT}"
echo | tee -a "${CMD_OUT}"
echo | tee -a "${CMD_OUT}"

"${CMD[@]}" 2>&1 | tee "${OUT}"

# ----------------------------------
# Parse stats
# ----------------------------------
echo "[STATS] parsing output..."
python3 "${TOOLS_DIR}/parse_gateway_stats.py" "${OUT}" | tee "${STATS_OUT}"

echo "[OK] pcap rx test saved: ${OUT}"
echo "[OK] stats: ${STATS_OUT}"
