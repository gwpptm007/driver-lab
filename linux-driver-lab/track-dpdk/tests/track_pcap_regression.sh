#!/usr/bin/env bash
set -euo pipefail

TRACK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
RUNTIME_DIR=${RUNTIME_DIR:-${TRACK_ROOT}/tests/runtime}
SKIP_BUILD=${SKIP_BUILD:-0}
mkdir -p "${RUNTIME_DIR}"

FAST_PROJECT="${TRACK_ROOT}/project-user-space-fastpath"
FAST_TEST="${TRACK_ROOT}/project-fastpath-traffic-test"
MEDIA_PROJECT="${TRACK_ROOT}/project-dpdk-media-gateway-lite"

if [[ "${SKIP_BUILD}" != "1" ]]; then
  # clean build 只触碰项目 build 目录，不绑定真实 PCI 设备。
  (cd "${FAST_PROJECT}" && ./scripts/01_build_app.sh)
  (cd "${MEDIA_PROJECT}" && RECORD_DIR="${RUNTIME_DIR}/media-build" ./scripts/01_build_app.sh)
fi

rm -rf "${RUNTIME_DIR}/fast-normal" "${RUNTIME_DIR}/fast-rewrite" \
  "${RUNTIME_DIR}/media-rewrite"
mkdir -p "${RUNTIME_DIR}/fast-normal" "${RUNTIME_DIR}/fast-rewrite" \
  "${RUNTIME_DIR}/media-rewrite"

# pcap + null PMD 是确定性软件功能回归，不操作物理 NIC。
(cd "${FAST_TEST}" && \
  RUN_SECONDS=2 STATS_PERIOD=1 RECORD_DIR="${RUNTIME_DIR}/fast-normal" \
  FILE_PREFIX="track_dpdk_fast_normal_$$" ./scripts/06_run_pcap_rx_test.sh)
grep -Eq 'verdict=.*PASS_TRAFFIC.*PASS_FORWARDING' \
  "${RUNTIME_DIR}/fast-normal/FASTPATH_PCAP_RX_STATS.txt"

(cd "${FAST_TEST}" && \
  RUN_SECONDS=2 STATS_PERIOD=1 REWRITE_ENABLE=1 \
  RECORD_DIR="${RUNTIME_DIR}/fast-rewrite" \
  FILE_PREFIX="track_dpdk_fast_rewrite_$$" ./scripts/06_run_pcap_rx_test.sh)
grep -Eq 'verdict=.*PASS_TRAFFIC.*PASS_FORWARDING.*PASS_REWRITE' \
  "${RUNTIME_DIR}/fast-rewrite/FASTPATH_PCAP_RX_STATS.txt"

MEDIA_ARGS='--rule0 0:1 --rule0-dst-port 9000 --rule0-rewrite-dst-ip 10.10.20.20 --rule0-rewrite-dst-mac 52:54:00:00:00:02 --rule0-rewrite-dst-port 10000'
(cd "${MEDIA_PROJECT}" && \
  MEDIA_RUN_SECONDS=2 MEDIA_STATS_PERIOD=1 MEDIA_SWAP_MAC=0 MEDIA_STRICT_RULES=0 \
  MEDIA_EXTRA_APP_ARGS="${MEDIA_ARGS}" RECORD_DIR="${RUNTIME_DIR}/media-rewrite" \
  MEDIA_FILE_PREFIX="track_dpdk_media_$$" ./scripts/06_run_pcap_rx_test.sh)
grep -q 'PASS_TRAFFIC=YES' "${RUNTIME_DIR}/media-rewrite/MEDIA_GATEWAY_PCAP_RX_TEST_STATS.txt"
grep -q 'PASS_FORWARDING=YES' "${RUNTIME_DIR}/media-rewrite/MEDIA_GATEWAY_PCAP_RX_TEST_STATS.txt"
grep -q 'PASS_REWRITE=YES' "${RUNTIME_DIR}/media-rewrite/MEDIA_GATEWAY_PCAP_RX_TEST_STATS.txt"

echo 'PASS_PCAP_FUNCTIONAL'
echo 'PASS_PCAP_FORWARDING'
echo 'PASS_PCAP_REWRITE'
echo 'DPDK_TRACK_PCAP_REGRESSION_PASS'
