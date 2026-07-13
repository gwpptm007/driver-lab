#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
. tests/perf_launch_helpers.sh

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_IB_PORT="${RDMA_IB_PORT:-1}"
RDMA_NETDEV="${RDMA_NETDEV:-ens33}"
RDMA_GID_INDEX="${RDMA_GID_INDEX:-1}"
PERF_SERVER_CPUSET="${PERF_SERVER_CPUSET:-}"
PERF_SERVER_NUMA_NODE="${PERF_SERVER_NUMA_NODE:-}"
SERVER_LISTEN="${SERVER_LISTEN:-0.0.0.0}"
TCP_PORT="${TCP_PORT:-18620}"
PERF_ITERATIONS="${PERF_ITERATIONS:-1000}"
PERF_BATCH_SIZE="${PERF_BATCH_SIZE:-8}"
PERF_USE_INLINE="${PERF_USE_INLINE:-0}"
PERF_SIGNAL_INTERVAL="${PERF_SIGNAL_INTERVAL:-1}"
PERF_POLL_CQ_BUDGET="${PERF_POLL_CQ_BUDGET:-16}"
PERF_ENABLE_RTT="${PERF_ENABLE_RTT:-0}"
ENABLE_TCPDUMP="${ENABLE_TCPDUMP:-1}"
TCPDUMP_COUNT="${TCPDUMP_COUNT:-20}"
TCPDUMP_TIMEOUT="${TCPDUMP_TIMEOUT:-20}"

SERVER_LOG="${SERVER_LOG:-tests/perf-server-dual.log}"
TCPDUMP_LOG="${TCPDUMP_LOG:-tests/perf-tcpdump-dual-4791.log}"

if [[ "${PERF_USE_INLINE}" == "1" ]]; then
  send_pass_server="PERF_SEND_LATENCY_INLINE_SERVER_PASS"
  batch_pass_server="PERF_BATCH_SEND_INLINE_SERVER_PASS"
  selective_pass_server="PERF_BATCH_SEND_INLINE_SELECTIVE_SERVER_PASS"
  rtt_pass_server="PERF_RTT_LATENCY_INLINE_SERVER_PASS"
else
  send_pass_server="PERF_SEND_LATENCY_SERVER_PASS"
  batch_pass_server="PERF_BATCH_SEND_SERVER_PASS"
  selective_pass_server="PERF_BATCH_SEND_SELECTIVE_SERVER_PASS"
  rtt_pass_server="PERF_RTT_LATENCY_SERVER_PASS"
fi

echo "script_config name=dual_perf_server listen=${SERVER_LISTEN} port=${TCP_PORT} device=${RDMA_DEVICE} ib_port=${RDMA_IB_PORT} netdev=${RDMA_NETDEV} gid_index=${RDMA_GID_INDEX} iterations=${PERF_ITERATIONS} batch_size=${PERF_BATCH_SIZE} inline=${PERF_USE_INLINE} signal_interval=${PERF_SIGNAL_INTERVAL} poll_budget=${PERF_POLL_CQ_BUDGET} enable_rtt=${PERF_ENABLE_RTT} tcpdump=${ENABLE_TCPDUMP} server_cpuset=${PERF_SERVER_CPUSET:-auto} server_numa=${PERF_SERVER_NUMA_NODE:-auto}"

run_sudo() {
  if [[ -n "${SUDO_PASSWORD:-}" ]]; then
    printf '%s\n' "${SUDO_PASSWORD}" | sudo -S "$@"
  else
    sudo "$@"
  fi
}

prepare_rxe() {
  # 双机 Soft-RoCE 要绑定到承载 192.168.65.x 的真实网卡，沿用 gid-index=1。
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

  # RoCEv2 走 UDP 4791，抓包仅用于证明链路确实出网。
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

perf_print_binding server
perf_make_launcher server
echo "script_case=dual_perf_server status=start log=${SERVER_LOG}"
PERF_ITERATIONS="${PERF_ITERATIONS}" PERF_BATCH_SIZE="${PERF_BATCH_SIZE}" PERF_USE_INLINE="${PERF_USE_INLINE}" PERF_SIGNAL_INTERVAL="${PERF_SIGNAL_INTERVAL}" PERF_POLL_CQ_BUDGET="${PERF_POLL_CQ_BUDGET}" PERF_ENABLE_RTT="${PERF_ENABLE_RTT}" \
  "${PERF_LAUNCH_CMD[@]}" ./build/rdma-perf-server \
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
  "${send_pass_server}" \
  "${batch_pass_server}" \
  'cleanup=complete result=pass'; do
  grep -q "${marker}" "${SERVER_LOG}"
done
grep -q 'perf_binding role=server' "${SERVER_LOG}"

if [[ "${PERF_SIGNAL_INTERVAL}" != "1" ]]; then
  grep -q "${selective_pass_server}" "${SERVER_LOG}"
fi

if [[ "${PERF_ENABLE_RTT}" == "1" ]]; then
  grep -q "${rtt_pass_server}" "${SERVER_LOG}"
  grep -q 'server_perf_rtt_recv_cqe' "${SERVER_LOG}"
  grep -q 'server_perf_rtt_send_cqe' "${SERVER_LOG}"
fi

if [[ "${ENABLE_TCPDUMP}" == "1" ]] && command -v tcpdump >/dev/null 2>&1; then
  grep -q 'UDP' "${TCPDUMP_LOG}"
fi

echo "PASS: dual-machine server side perf path"
echo "script_case=dual_perf_server status=pass"
