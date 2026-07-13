#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
source tests/launch_helpers.sh

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_IB_PORT="${RDMA_IB_PORT:-1}"
RDMA_NETDEV="${RDMA_NETDEV:-ens33}"
RDMA_GID_INDEX="${RDMA_GID_INDEX:-1}"
SERVER_LISTEN="${SERVER_LISTEN:-0.0.0.0}"
TCP_PORT="${TCP_PORT:-18520}"
ENABLE_TCPDUMP="${ENABLE_TCPDUMP:-1}"
TCPDUMP_COUNT="${TCPDUMP_COUNT:-20}"
TCPDUMP_TIMEOUT="${TCPDUMP_TIMEOUT:-20}"

SERVER_LOG="${SERVER_LOG:-tests/server-dual.log}"
TCPDUMP_LOG="${TCPDUMP_LOG:-tests/tcpdump-dual-4791.log}"

echo "script_config name=dual_server_capture listen=${SERVER_LISTEN} port=${TCP_PORT} device=${RDMA_DEVICE} ib_port=${RDMA_IB_PORT} netdev=${RDMA_NETDEV} gid_index=${RDMA_GID_INDEX} tcpdump=${ENABLE_TCPDUMP}"

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

prepare_rxe() {
  # 双机 RoCEv2 需要把 RXE 绑定到承载 192.168.65.x 的真实网卡。
  echo "script_step=prepare_rxe role=server status=start netdev=${RDMA_NETDEV} device=${RDMA_DEVICE}"
  run_sudo modprobe rdma_rxe >/dev/null 2>&1 || true
  run_sudo rdma link delete "${RDMA_DEVICE}" >/dev/null 2>&1 || true
  run_sudo rdma link add "${RDMA_DEVICE}" type rxe netdev "${RDMA_NETDEV}" >/dev/null
  echo "script_step=prepare_rxe role=server status=done"
}

start_tcpdump() {
  if [[ "${ENABLE_TCPDUMP}" != "1" ]]; then
    echo "script_step=tcpdump role=server status=disabled"
    return
  fi
  if ! command -v tcpdump >/dev/null 2>&1; then
    echo "SKIP: tcpdump not installed" | tee "${TCPDUMP_LOG}"
    echo "script_step=tcpdump role=server status=missing"
    return
  fi

  # RoCEv2 使用 UDP 4791。抓包只作为证据，不参与程序控制面。
  echo "script_step=tcpdump role=server status=start log=${TCPDUMP_LOG}"
  run_sudo timeout "${TCPDUMP_TIMEOUT}" \
    tcpdump -ni "${RDMA_NETDEV}" udp port 4791 -c "${TCPDUMP_COUNT}" \
    > "${TCPDUMP_LOG}" 2>&1 &
  tcpdump_pid=$!
  echo "script_step=tcpdump role=server status=running pid=${tcpdump_pid}"
}

wait_tcpdump() {
  if [[ -n "${tcpdump_pid:-}" ]]; then
    wait "${tcpdump_pid}" 2>/dev/null || true
  fi
}

make
prepare_rxe

echo "server_netdev=${RDMA_NETDEV}"
rdma link
ibv_devinfo -d "${RDMA_DEVICE}" -v | sed -n '/GID\[/,+2p' | head -20

rm -f "${SERVER_LOG}" "${TCPDUMP_LOG}"
start_tcpdump

echo "script_case=dual_server status=start log=${SERVER_LOG}"
run_server \
  --listen "${SERVER_LISTEN}" \
  --port "${TCP_PORT}" \
  --device "${RDMA_DEVICE}" \
  --ib-port "${RDMA_IB_PORT}" \
  --gid-index "${RDMA_GID_INDEX}" \
  > "${SERVER_LOG}" 2>&1

wait_tcpdump
echo "script_step=tcpdump role=server status=done log=${TCPDUMP_LOG}"

for marker in \
  TCP_CONTROL_PLANE_PASS \
  RC_QP_RTS_PASS \
  RC_SEND_RECV_PASS \
  RDMA_WRITE_PASS \
  RDMA_READ_PASS \
  WRONG_RKEY_BOUNDARY_PASS \
  'cleanup=complete result=pass'; do
  grep -q "${marker}" "${SERVER_LOG}"
done

if [[ "${ENABLE_TCPDUMP}" == "1" ]] && command -v tcpdump >/dev/null 2>&1; then
  grep -q 'UDP' "${TCPDUMP_LOG}"
fi

echo "PASS: dual-machine server side RoCEv2 path"
echo "script_case=dual_server status=pass"
