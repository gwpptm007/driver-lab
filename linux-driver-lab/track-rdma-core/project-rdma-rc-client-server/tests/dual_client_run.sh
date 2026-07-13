#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
source tests/launch_helpers.sh

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_IB_PORT="${RDMA_IB_PORT:-1}"
RDMA_NETDEV="${RDMA_NETDEV:-ens33}"
RDMA_GID_INDEX="${RDMA_GID_INDEX:-1}"
SERVER_IP="${SERVER_IP:-192.168.65.135}"
TCP_PORT="${TCP_PORT:-18520}"
CLIENT_LOG="${CLIENT_LOG:-tests/client-dual.log}"

echo "script_config name=dual_client_run server=${SERVER_IP} port=${TCP_PORT} device=${RDMA_DEVICE} ib_port=${RDMA_IB_PORT} netdev=${RDMA_NETDEV} gid_index=${RDMA_GID_INDEX}"

run_sudo() {
  if [[ -n "${SUDO_PASSWORD:-}" ]]; then
    printf '%s\n' "${SUDO_PASSWORD}" | sudo -S "$@"
  else
    sudo "$@"
  fi
}

run_client() {
  rdma_print_binding client
  rdma_make_launcher client
  "${RDMA_LAUNCH_CMD[@]}" ./build/rdma-rc-client "$@"
}

prepare_rxe() {
  # client 侧同样绑定到 192.168.65.x 所在网卡，默认使用 IPv4-mapped GID[1]。
  echo "script_step=prepare_rxe role=client status=start netdev=${RDMA_NETDEV} device=${RDMA_DEVICE}"
  run_sudo modprobe rdma_rxe >/dev/null 2>&1 || true
  run_sudo rdma link delete "${RDMA_DEVICE}" >/dev/null 2>&1 || true
  run_sudo rdma link add "${RDMA_DEVICE}" type rxe netdev "${RDMA_NETDEV}" >/dev/null
  echo "script_step=prepare_rxe role=client status=done"
}

make
prepare_rxe

echo "client_netdev=${RDMA_NETDEV}"
rdma link
ibv_devinfo -d "${RDMA_DEVICE}" -v | sed -n '/GID\[/,+2p' | head -20

rm -f "${CLIENT_LOG}"
echo "script_case=dual_client status=start log=${CLIENT_LOG}"
run_client \
  --server "${SERVER_IP}" \
  --port "${TCP_PORT}" \
  --device "${RDMA_DEVICE}" \
  --ib-port "${RDMA_IB_PORT}" \
  --gid-index "${RDMA_GID_INDEX}" \
  > "${CLIENT_LOG}" 2>&1

for marker in \
  TCP_CONTROL_PLANE_PASS \
  RC_QP_RTS_PASS \
  RC_SEND_RECV_PASS \
  RDMA_WRITE_PASS \
  RDMA_READ_PASS \
  WRONG_RKEY_BOUNDARY_PASS \
  'cleanup=complete result=pass'; do
  grep -q "${marker}" "${CLIENT_LOG}"
done

echo "PASS: dual-machine client side RC data path"
echo "script_case=dual_client status=pass"
