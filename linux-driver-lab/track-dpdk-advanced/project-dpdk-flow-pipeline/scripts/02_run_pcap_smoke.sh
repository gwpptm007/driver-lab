#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

python3 "${TOOLS_DIR}/gen_flow_pcap.py" "${FLOW_PCAP_FILE}" "${FLOW_PCAP_COUNT}"
# 两个 vdev 分别承担确定性 RX 输入和 TX sink，应用参数放在 EAL 的 -- 之后。
"${APP_BIN}" \
  -l "${FLOW_LCORES}" -n "${FLOW_MEMORY_CHANNELS}" --no-pci --no-huge \
  --file-prefix "${FLOW_FILE_PREFIX}_$$" \
  --vdev "net_pcap0,rx_pcap=${FLOW_PCAP_FILE}" \
  --vdev "net_null1" \
  -- \
  --burst-size "${FLOW_BURST_SIZE}" \
  --mbuf-cache "${FLOW_MBUF_CACHE}" \
  --expected-packets "${FLOW_PCAP_COUNT}" \
  --extra-rules "${FLOW_EXTRA_RULES}" \
  --max-idle-polls "${FLOW_MAX_IDLE_POLLS}" | tee "${FLOW_LOG_FILE}"
