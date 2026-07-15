#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "${PROJECT_ROOT}"
mkdir -p tests/runtime

# 使用固定 3:1 流量比例，联合断言 parser、staging、mock completion 计数守恒。
python3 tools/gen_gateway_pcap.py tests/runtime/gateway_phase2.pcap 64
make dpdk
./build/gateway-dpdk-ingress \
  -l 0 -n 4 --no-pci --no-huge \
  --file-prefix "dpdk_rdma_gateway_phase2_$$" \
  --vdev "net_pcap0,rx_pcap=tests/runtime/gateway_phase2.pcap" \
  -- --expected-packets 64 | tee tests/runtime/phase2_ingress.log

LOG=tests/runtime/phase2_ingress.log
grep -q 'GATEWAY_INGRESS_RESULT rx=64 udp=48 unsupported=16 malformed=0 staged=48 ring_full=0 slot_exhausted=0' "${LOG}"
grep -q 'GATEWAY_MOCK_RDMA_RESULT dequeued=48 completed=48 payload_bytes=1536' "${LOG}"
grep -q 'DPDK_RDMA_GATEWAY_PHASE2_INGRESS_PASS' "${LOG}"
grep -q 'cleanup=complete result=pass' "${LOG}"

echo 'PASS: DPDK-RDMA gateway Phase 2 pcap ingress and staging'
echo 'script_summary name=phase2_ingress_test status=pass'
