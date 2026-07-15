#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "${PROJECT_ROOT}"
mkdir -p tests/runtime

RDMA_DEVICE=${RDMA_DEVICE:-rxe0}
RDMA_GID_INDEX=${RDMA_GID_INDEX:-1}
RDMA_PORT=${RDMA_PORT:-18615}
SERVER_LOG=tests/runtime/phase3_server.log
CLIENT_LOG=tests/runtime/phase3_client.log
source tests/rdma_test_env.sh

cleanup() {
  if [[ -n "${server_pid:-}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

# 默认仅检查 RXE；显式设置 GATEWAY_PREPARE_RXE=1 时才执行可重复的环境准备。
gateway_check_rdma_env
make rdma

timeout 20s ./build/gateway-rdma-server \
  --listen 127.0.0.1 --port "${RDMA_PORT}" --device "${RDMA_DEVICE}" \
  --ib-port 1 --gid-index "${RDMA_GID_INDEX}" >"${SERVER_LOG}" 2>&1 &
server_pid=$!
sleep 1
timeout 20s ./build/gateway-rdma-client \
  --server 127.0.0.1 --port "${RDMA_PORT}" --device "${RDMA_DEVICE}" \
  --ib-port 1 --gid-index "${RDMA_GID_INDEX}" >"${CLIENT_LOG}" 2>&1
wait "${server_pid}"
trap - EXIT

grep -q 'GATEWAY_RDMA_QP_RTS_PASS role=server' "${SERVER_LOG}"
grep -q 'GATEWAY_RDMA_SERVER_READY' "${SERVER_LOG}"
grep -q 'GATEWAY_REMOTE_RECORD_PASS request_id=3001 payload_bytes=32' "${SERVER_LOG}"
grep -q 'DPDK_RDMA_GATEWAY_PHASE3_SERVER_PASS' "${SERVER_LOG}"
grep -q 'cleanup=complete role=server result=pass' "${SERVER_LOG}"
grep -q 'GATEWAY_RDMA_QP_RTS_PASS role=client' "${CLIENT_LOG}"
grep -q 'gateway_rdma_write_cqe cqe_wr_id=3001 status=success' "${CLIENT_LOG}"
grep -q 'GATEWAY_RDMA_WRITE_CQE_PASS request_id=3001 bytes=72' "${CLIENT_LOG}"
grep -q 'GATEWAY_RDMA_SLOT_COMPLETE_PASS slot=3 generation=1' "${CLIENT_LOG}"
grep -q 'DPDK_RDMA_GATEWAY_PHASE3_RDMA_PASS' "${CLIENT_LOG}"
grep -q 'cleanup=complete role=client result=pass' "${CLIENT_LOG}"

echo 'DPDK_RDMA_GATEWAY_PHASE3_RDMA_PASS'
echo 'PASS: DPDK-RDMA gateway Phase 3 RXE WRITE and CQE'
echo 'script_summary name=phase3_rdma_test status=pass'
