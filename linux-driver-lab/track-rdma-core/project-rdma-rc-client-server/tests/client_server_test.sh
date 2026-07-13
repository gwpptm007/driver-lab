#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
source tests/launch_helpers.sh

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_IB_PORT="${RDMA_IB_PORT:-1}"
RDMA_NETDEV="${RDMA_NETDEV:-ens34}"
RDMA_GID_ADDR="${RDMA_GID_ADDR:-fe80::34}"
RDMA_GID_INDEX="${RDMA_GID_INDEX:-1}"

echo "script_config name=client_server_test device=${RDMA_DEVICE} ib_port=${RDMA_IB_PORT} netdev=${RDMA_NETDEV} gid_addr=${RDMA_GID_ADDR} gid_index=${RDMA_GID_INDEX}"

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
  "${RDMA_LAUNCH_CMD[@]}" ./build/rdma-rc-server "$@"
}

run_client() {
  rdma_print_binding client
  rdma_make_launcher client
  "${RDMA_LAUNCH_CMD[@]}" ./build/rdma-rc-client "$@"
}

make clean
make

echo 'script_case=control_plane status=start server_log=tests/server.log client_log=tests/client.log'
run_server --control-plane-only --listen 127.0.0.1 --port 18515 \
  > tests/server.log 2>&1 &
server_pid=$!

cleanup() {
  if kill -0 "${server_pid}" 2>/dev/null; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

sleep 1
run_client --control-plane-only --server 127.0.0.1 --port 18515 \
  > tests/client.log 2>&1
wait "${server_pid}"
trap - EXIT

grep -q 'server_control_plane=pass' tests/server.log
grep -q 'client_control_plane=pass' tests/client.log
grep -q 'TCP_CONTROL_PLANE_PASS' tests/server.log
grep -q 'TCP_CONTROL_PLANE_PASS' tests/client.log
grep -q 'app_config role=server' tests/server.log
grep -q 'app_config role=client' tests/client.log
grep -q 'app_runtime_binding role=server' tests/server.log
grep -q 'app_runtime_binding role=client' tests/client.log

echo 'PASS: TCP control plane metadata exchange'
echo 'script_case=control_plane status=pass'

echo "script_step=prepare_rxe status=start netdev=${RDMA_NETDEV} device=${RDMA_DEVICE} gid_addr=${RDMA_GID_ADDR}"
run_sudo modprobe rdma_rxe >/dev/null 2>&1 || true
run_sudo ip -6 addr add "${RDMA_GID_ADDR}/64" dev "${RDMA_NETDEV}" >/dev/null 2>&1 || true
run_sudo rdma link delete "${RDMA_DEVICE}" >/dev/null 2>&1 || true
run_sudo rdma link add "${RDMA_DEVICE}" type rxe netdev "${RDMA_NETDEV}" >/dev/null 2>&1 || true
while ip -6 addr show "${RDMA_NETDEV}" | grep -q tentative; do sleep 1; done
echo 'script_step=prepare_rxe status=done'

echo 'script_case=dry_run status=start server_log=tests/server-dry-run.log client_log=tests/client-dry-run.log'
run_server --dry-run --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" \
  > tests/server-dry-run.log 2>&1
run_client --dry-run --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" \
  > tests/client-dry-run.log 2>&1

grep -q 'rdma_resources=created' tests/server-dry-run.log
grep -q 'rdma_resources=created' tests/client-dry-run.log
grep -q 'phase=resources_create role=server' tests/server-dry-run.log
grep -q 'phase=resources_create role=client' tests/client-dry-run.log
grep -q 'cleanup=complete result=pass' tests/server-dry-run.log
grep -q 'cleanup=complete result=pass' tests/client-dry-run.log
grep -q 'app_runtime_binding role=server' tests/server-dry-run.log
grep -q 'app_runtime_binding role=client' tests/client-dry-run.log

echo 'PASS: RDMA resource lifecycle dry-run'
echo 'script_case=dry_run status=pass'

echo 'script_case=full_wrong_rkey status=start server_log=tests/server-full.log client_log=tests/client-full.log'
run_server --listen 127.0.0.1 --port 18516 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" \
  > tests/server-full.log 2>&1 &
server_pid=$!
trap cleanup EXIT

sleep 1
run_client --server 127.0.0.1 --port 18516 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" \
  > tests/client-full.log 2>&1
wait "${server_pid}"
trap - EXIT

for marker in \
  TCP_CONTROL_PLANE_PASS \
  RC_QP_RTS_PASS \
  RC_SEND_RECV_PASS \
  RDMA_WRITE_PASS \
  RDMA_READ_PASS \
  WRONG_RKEY_BOUNDARY_PASS \
  'cleanup=complete result=pass'; do
  grep -q "${marker}" tests/server-full.log
  grep -q "${marker}" tests/client-full.log
done
grep -q 'app_runtime_binding role=server' tests/server-full.log
grep -q 'app_runtime_binding role=client' tests/client-full.log

echo 'PASS: RC client/server SEND WRITE READ wrong-rkey'
echo 'script_case=full_wrong_rkey status=pass'

echo 'script_case=wrong_addr status=start server_log=tests/server-wrong-addr.log client_log=tests/client-wrong-addr.log'
run_server --listen 127.0.0.1 --port 18519 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" --wrong-addr \
  > tests/server-wrong-addr.log 2>&1 &
server_pid=$!
trap cleanup EXIT

sleep 1
run_client --server 127.0.0.1 --port 18519 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" --wrong-addr \
  > tests/client-wrong-addr.log 2>&1
wait "${server_pid}"
trap - EXIT

grep -q 'WRONG_ADDR_BOUNDARY_PASS' tests/server-wrong-addr.log
grep -q 'WRONG_ADDR_BOUNDARY_PASS' tests/client-wrong-addr.log
grep -q 'cleanup=complete result=pass' tests/server-wrong-addr.log
grep -q 'cleanup=complete result=pass' tests/client-wrong-addr.log

echo 'PASS: wrong-addr remote address boundary'
echo 'script_case=wrong_addr status=pass'

echo 'script_case=skip_recv status=start server_log=tests/server-skip-recv.log client_log=tests/client-skip-recv.log'
run_server --listen 127.0.0.1 --port 18517 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" --skip-recv \
  > tests/server-skip-recv.log 2>&1 &
server_pid=$!
trap cleanup EXIT

sleep 1
run_client --server 127.0.0.1 --port 18517 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" --skip-recv \
  > tests/client-skip-recv.log 2>&1
wait "${server_pid}"
trap - EXIT

grep -q 'SKIP_RECV_BOUNDARY_PASS' tests/server-skip-recv.log
grep -q 'SKIP_RECV_BOUNDARY_PASS' tests/client-skip-recv.log
grep -q 'cleanup=complete result=pass' tests/server-skip-recv.log
grep -q 'cleanup=complete result=pass' tests/client-skip-recv.log

echo 'PASS: skip-recv RNR boundary'
echo 'script_case=skip_recv status=pass'

echo 'script_case=disconnect_after_rts status=start server_log=tests/server-disconnect.log client_log=tests/client-disconnect.log'
run_server --listen 127.0.0.1 --port 18518 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" --disconnect-after-rts \
  > tests/server-disconnect.log 2>&1 &
server_pid=$!
trap cleanup EXIT

sleep 1
run_client --server 127.0.0.1 --port 18518 \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" --disconnect-after-rts \
  > tests/client-disconnect.log 2>&1
wait "${server_pid}"
trap - EXIT

grep -q 'DISCONNECT_AFTER_RTS_PASS' tests/server-disconnect.log
grep -q 'DISCONNECT_AFTER_RTS_PASS' tests/client-disconnect.log
grep -q 'cleanup=complete result=pass' tests/server-disconnect.log
grep -q 'cleanup=complete result=pass' tests/client-disconnect.log

echo 'PASS: disconnect-after-rts cleanup boundary'
echo 'script_case=disconnect_after_rts status=pass'
echo 'script_summary name=client_server_test status=pass'
