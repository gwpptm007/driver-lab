#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

if [[ -n "${1:-}" ]]; then
  RECORD_DIR="$1"
fi

LOG="${RECORD_DIR}/PCAP_QUEUE_PROBE.log"
REPORT="${RECORD_DIR}/SUMMARY.md"

if [[ ! -f "${LOG}" ]]; then
  echo "[ERR] missing log: ${LOG}" >&2
  exit 1
fi

queue_result=$(grep -Eo 'PASS_QUEUE_CONFIG|BLOCKED_QUEUE_CONFIG' "${LOG}" | tail -1 || echo "BLOCKED_QUEUE_CONFIG")
rss_result=$(grep -Eo 'PASS_RSS_QUERY|BLOCKED_RSS' "${LOG}" | tail -1 || echo "BLOCKED_RSS")
map_result=$(grep -Eo 'PASS_QUEUE_TO_CORE_DOC' "${LOG}" | tail -1 || echo "CHECK_QUEUE_TO_CORE_DOC")
driver=$(grep -Eo 'driver_name=[^ ]+' "${LOG}" | tail -1 | cut -d= -f2 || echo "unknown")
max_rx=$(grep -Eo 'max_rx_queues=[0-9]+' "${LOG}" | tail -1 | cut -d= -f2 || echo "0")
reta=$(grep -Eo 'reta_size=[0-9]+' "${LOG}" | tail -1 | cut -d= -f2 || echo "0")
rss_hex=$(grep -Eo 'rss_offloads_hex=0x[0-9a-fA-F]+' "${LOG}" | tail -1 | cut -d= -f2 || echo "0x0")

{
  echo "# RSS / multiqueue Phase 2 Summary"
  echo
  echo "| Item | Result |"
  echo "|------|--------|"
  echo "| PASS_BUILD | $(test -f "${RECORD_DIR}/BUILD.log" && echo PASS || echo CHECK) |"
  echo "| QUEUE_CONFIG | ${queue_result} |"
  echo "| RSS_QUERY | ${rss_result} |"
  echo "| QUEUE_TO_CORE_DOC | ${map_result} |"
  echo
  echo "## Capability"
  echo
  echo "- driver_name=${driver}"
  echo "- max_rx_queues=${max_rx}"
  echo "- reta_size=${reta}"
  echo "- rss_offloads=${rss_hex}"
  echo
  echo "## Queue map"
  echo
  grep '^queue_map ' "${LOG}" || true
  echo
  echo "## Blocked reasons"
  echo
  grep '^blocked_reason=' "${LOG}" || true
} > "${REPORT}"

echo "[OK] report saved: ${REPORT}"
