#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "${PROJECT_ROOT}"
mkdir -p tests/runtime

RDMA_DEVICE=${RDMA_DEVICE:-rxe0}
RDMA_GID_INDEX=${RDMA_GID_INDEX:-1}
RDMA_PORT=${RDMA_PORT:-18616}
SERVER_LOG=tests/runtime/phase4_server.log
CLIENT_LOG=tests/runtime/phase4_client.log
source tests/rdma_test_env.sh

cleanup() {
  if [[ -n "${server_pid:-}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

# 端到端测试复用同一份 3 UDP / 1 ICMP pcap，并检查目标 RXE GID。
gateway_check_rdma_env
python3 tools/gen_gateway_pcap.py tests/runtime/gateway_phase4.pcap 64
make integrated

timeout 30s ./build/gateway-integrated-server \
  --listen 127.0.0.1 --port "${RDMA_PORT}" --device "${RDMA_DEVICE}" \
  --ib-port 1 --gid-index "${RDMA_GID_INDEX}" >"${SERVER_LOG}" 2>&1 &
server_pid=$!
sleep 1
timeout 30s ./build/gateway-integrated-client \
  -l 0 -n 4 --no-pci --no-huge \
  --file-prefix "dpdk_rdma_gateway_phase4_$$" \
  --vdev "net_pcap0,rx_pcap=tests/runtime/gateway_phase4.pcap" -- \
  --expected-packets 64 --server 127.0.0.1 --port "${RDMA_PORT}" \
  --device "${RDMA_DEVICE}" --ib-port 1 --gid-index "${RDMA_GID_INDEX}" \
  >"${CLIENT_LOG}" 2>&1
wait "${server_pid}"
trap - EXIT

grep -q 'GATEWAY_E2E_SERVER_READY' "${SERVER_LOG}"
grep -q 'GATEWAY_E2E_REMOTE_LAST_RECORD_PASS request_id=48 payload=GATEWAY_UDP_0062' "${SERVER_LOG}"
grep -q 'DPDK_RDMA_GATEWAY_PHASE4_SERVER_PASS' "${SERVER_LOG}"
grep -q 'cleanup=complete role=e2e_server result=pass' "${SERVER_LOG}"
grep -q 'GATEWAY_E2E_QP_RTS_PASS' "${CLIENT_LOG}"
grep -q 'GATEWAY_E2E_INGRESS_RESULT rx=64 udp=48 unsupported=16 staged=48 ring_full=0 slot_exhausted=0' "${CLIENT_LOG}"
grep -q 'GATEWAY_E2E_RDMA_RESULT dequeued=48 completed=48 payload_bytes=1536 write_bytes=3456 errors=0 active_slots=0' "${CLIENT_LOG}"
grep -q 'DPDK_RDMA_GATEWAY_PHASE4_E2E_PASS' "${CLIENT_LOG}"
grep -q 'cleanup=complete role=e2e_client result=pass' "${CLIENT_LOG}"

echo 'DPDK_RDMA_GATEWAY_PHASE4_E2E_PASS'
echo 'PASS: DPDK-RDMA gateway Phase 4 integrated pcap-to-RDMA path'
echo 'script_summary name=phase4_e2e_test status=pass'
