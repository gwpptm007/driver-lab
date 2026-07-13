#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
source tests/launch_helpers.sh

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_IB_PORT="${RDMA_IB_PORT:-1}"
RDMA_NETDEV="${RDMA_NETDEV:-ens34}"
RDMA_GID_ADDR="${RDMA_GID_ADDR:-fe80::34}"
RDMA_GID_INDEX="${RDMA_GID_INDEX:-1}"

run_sudo() {
  if [[ -n "${SUDO_PASSWORD:-}" ]]; then
    printf '%s\n' "${SUDO_PASSWORD}" | sudo -S "$@"
  else
    sudo "$@"
  fi
}

run_server() {
  rdma_print_binding server
  rdma_make_launcher server
  "${RDMA_LAUNCH_CMD[@]}" ./build/rdma-kv-server "$@"
}

run_client() {
  rdma_print_binding client
  rdma_make_launcher client
  "${RDMA_LAUNCH_CMD[@]}" ./build/rdma-kv-client "$@"
}

cleanup() {
  if [[ -n "${server_pid:-}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
}

echo "script_config name=kv_smoke_test device=${RDMA_DEVICE} ib_port=${RDMA_IB_PORT} netdev=${RDMA_NETDEV} gid_addr=${RDMA_GID_ADDR} gid_index=${RDMA_GID_INDEX}"
make clean
make

echo "script_step=prepare_rxe status=start device=${RDMA_DEVICE}"
run_sudo modprobe rdma_rxe >/dev/null 2>&1 || true
run_sudo ip -6 addr add "${RDMA_GID_ADDR}/64" dev "${RDMA_NETDEV}" >/dev/null 2>&1 || true
run_sudo rdma link delete "${RDMA_DEVICE}" >/dev/null 2>&1 || true
run_sudo rdma link add "${RDMA_DEVICE}" type rxe netdev "${RDMA_NETDEV}" >/dev/null 2>&1 || true
while ip -6 addr show "${RDMA_NETDEV}" | grep -q tentative; do sleep 1; done
echo "script_step=prepare_rxe status=done device=${RDMA_DEVICE}"

echo "script_case=one_sided_kv status=start server_log=tests/kv-server.log client_log=tests/kv-client.log"
run_server --listen 127.0.0.1 --port 18525 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" \
  > tests/kv-server.log 2>&1 &
server_pid=$!
trap cleanup EXIT

sleep 1
run_client --server 127.0.0.1 --port 18525 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" \
  > tests/kv-client.log 2>&1
wait "${server_pid}"
trap - EXIT

for marker in \
  KV_SERVER_READY \
  KV_QP_RTS_PASS \
  KV_WRITE_PASS \
  KV_READ_PASS \
  KV_CREDIT_PASS \
  KV_REMOTE_ATOMIC_ACQUIRE_PASS \
  KV_BATCH_WRITE_PASS \
  KV_BATCH_READ_PASS \
  KV_REMOTE_ATOMIC_RETURN_PASS \
  KV_REMOTE_ATOMIC_CREDIT_PASS \
  KV_CAS_HOLDER_ACQUIRE_PASS \
  KV_CAS_CONTENDER_REJECT_PASS \
  KV_CAS_RETRY_ACQUIRE_PASS \
  KV_CAS_CREDIT_RECOVERY_PASS \
  KV_CAS_CONTENTION_PASS \
  KV_DYNAMIC_KEY_PUT_PASS \
  KV_DYNAMIC_KEY_GET_PASS \
  KV_DIRECTORY_COLLISION_PASS \
  KV_DYNAMIC_DIRECTORY_PASS \
  KV_RKEY_ROTATION_ACCESS_PASS \
  KV_STALE_RKEY_REJECT_PASS \
  KV_RKEY_BOUNDARY_PASS \
  ONE_SIDED_KV_PASS \
  ONE_SIDED_KV_CURRENT_ENV_COMPLETE \
  'cleanup=complete result=pass'; do
  grep -q "${marker}" tests/kv-server.log || [[ "${marker}" == "KV_QP_RTS_PASS" ]]
  grep -q "${marker}" tests/kv-client.log || [[ "${marker}" == "KV_SERVER_READY" ]]
done

grep -q 'kv_write_record slot=2 key=client-key-2' tests/kv-server.log
grep -q 'kv_read_record slot=2 key=client-key-2' tests/kv-client.log
grep -q 'kv_batch_write first_slot=3 count=4 tail_key=client-credit-key-6' tests/kv-server.log
grep -q 'kv_batch_read first_slot=3 count=4 tail_key=client-credit-key-6' tests/kv-client.log
grep -q 'kv_atomic_credit_final=4' tests/kv-server.log
grep -q 'kv_atomic_acquire old=4 add=-4' tests/kv-client.log
grep -q 'kv_atomic_return old=0 add=4' tests/kv-client.log
grep -q 'kv_cas_holder old=4 compare=4 swap=0' tests/kv-client.log
grep -q 'kv_cas_contender old=0 compare=4 swap=0' tests/kv-client.log
grep -q 'kv_cas_retry old=4 compare=4 swap=0' tests/kv-client.log
grep -q 'kv_cas_credit_final=4' tests/kv-server.log
grep -q 'kv_directory_put key=dynamic-alpha' tests/kv-client.log
grep -q 'kv_directory_get key=dynamic-alpha' tests/kv-client.log
grep -q 'kv_directory_collision existing=dynamic-alpha rejected=dynamic-collision-' tests/kv-client.log
grep -q 'kv_directory_server key=dynamic-alpha' tests/kv-server.log
grep -q 'kv_rkey_rotate old=0x' tests/kv-client.log
grep -q 'kv_client_rotated_rkey_write_cqe.*status=success' tests/kv-client.log
grep -q 'kv_client_rotated_rkey_read_cqe.*status=success' tests/kv-client.log
grep -q 'kv_client_stale_rkey_write_cqe.*status=remote access error' tests/kv-client.log
grep -q 'app_runtime_binding role=server' tests/kv-server.log
grep -q 'app_runtime_binding role=client' tests/kv-client.log

echo 'PASS: RDMA one-sided KV rkey rotation dynamic directory CAS atomic batch WRITE READ'
echo 'project_status=ONE_SIDED_KV_CURRENT_ENV_COMPLETE'
echo 'script_summary name=kv_smoke_test status=pass'
