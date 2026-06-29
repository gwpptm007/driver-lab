#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

if [[ -n "${1:-}" ]]; then
  RECORD_DIR="$1"
fi

LOG="${RECORD_DIR}/PCAP_METADATA.log"
REPORT="${RECORD_DIR}/SUMMARY.md"
mkdir -p "${RECORD_DIR}"

if [[ ! -f "${LOG}" ]]; then
  echo "[ERR] missing log: ${LOG}" >&2
  exit 1
fi

rx=$(grep -Eo 'software_rx_packets=[0-9]+' "${LOG}" | tail -1 | cut -d= -f2 || echo 0)
samples=$(grep -Eo 'samples_printed=[0-9]+' "${LOG}" | tail -1 | cut -d= -f2 || echo 0)
stats=$(grep -Eo 'PASS_STATS_CONSISTENCY|CHECK_STATS_CONSISTENCY' "${LOG}" | tail -1 || echo "CHECK_STATS_CONSISTENCY")
if [[ "${stats}" == "PASS_STATS_CONSISTENCY" ]]; then
  stats_result="PASS"
else
  stats_result="CHECK"
fi

{
  echo "# mbuf/mempool Phase 1 Summary"
  echo
  echo "| Item | Result |"
  echo "|------|--------|"
  echo "| PASS_BUILD | $(test -f "${RECORD_DIR}/BUILD.log" && echo PASS || echo CHECK) |"
  echo "| PASS_PCAP_RX | $(test "${rx}" -gt 0 && echo PASS || echo FAIL) |"
  echo "| PASS_MBUF_METADATA | $(test "${samples}" -gt 0 && echo PASS || echo FAIL) |"
  echo "| PASS_MEMPOOL_CONFIG | $(grep -q 'mempool_size=' "${LOG}" && echo PASS || echo FAIL) |"
  echo "| PASS_STATS_CONSISTENCY | ${stats_result} |"
  echo
  echo "## Counters"
  echo
  echo "- software_rx_packets=${rx}"
  echo "- samples_printed=${samples}"
  echo
  echo "## Key Samples"
  echo
  grep '^MBUF_SAMPLE index=' "${LOG}" | head -20 || true
} > "${REPORT}"

echo "[OK] report saved: ${REPORT}"
