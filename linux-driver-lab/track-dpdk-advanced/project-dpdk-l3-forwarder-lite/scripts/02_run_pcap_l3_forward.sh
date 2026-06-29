#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_bin
OUT="${RECORD_DIR}/L3_FORWARD.log"
PCAP_LOG="${RECORD_DIR}/PCAP_GENERATE.log"
{
  echo "# PCAP_GENERATE"; echo; log_env; echo
  python3 "${TOOLS_DIR}/gen_l3_pcap.py" "${PCAP_FILE}" "${L3_PCAP_COUNT}"
} 2>&1 | tee "$PCAP_LOG"

{
  echo "# L3_FORWARD"; echo; log_env; echo
  set -x
  "${APP_BIN}" \
    -l "${L3_LCORES}" -n "${L3_MEMORY_CHANNELS}" --no-pci \
    --file-prefix "${L3_FILE_PREFIX}_$(date +%s)" \
    --vdev "net_pcap0,rx_pcap=${PCAP_FILE}" \
    --vdev "net_null1" \
    -- \
    --burst-size "${L3_BURST_SIZE}" \
    --mbuf-cache "${L3_MBUF_CACHE}" \
    --max-idle-polls "${L3_MAX_IDLE_POLLS}"
  set +x
} 2>&1 | tee "$OUT"
echo "[OK] l3 forward saved: $OUT"

