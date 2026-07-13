#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
. tests/perf_launch_helpers.sh

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_IB_PORT="${RDMA_IB_PORT:-1}"
RDMA_NETDEV="${RDMA_NETDEV:-ens33}"
RDMA_GID_INDEX="${RDMA_GID_INDEX:-1}"
PERF_CLIENT_CPUSET="${PERF_CLIENT_CPUSET:-}"
PERF_CLIENT_NUMA_NODE="${PERF_CLIENT_NUMA_NODE:-}"
SERVER_IP="${SERVER_IP:-192.168.65.135}"
TCP_PORT="${TCP_PORT:-18620}"
PERF_ITERATIONS="${PERF_ITERATIONS:-1000}"
PERF_BATCH_SIZE="${PERF_BATCH_SIZE:-8}"
PERF_USE_INLINE="${PERF_USE_INLINE:-0}"
PERF_SIGNAL_INTERVAL="${PERF_SIGNAL_INTERVAL:-1}"
PERF_POLL_CQ_BUDGET="${PERF_POLL_CQ_BUDGET:-16}"
PERF_ENABLE_RTT="${PERF_ENABLE_RTT:-0}"
CLIENT_LOG="${CLIENT_LOG:-tests/perf-client-dual.log}"

if [[ "${PERF_USE_INLINE}" == "1" ]]; then
  send_test_name="send_latency_inline"
  batch_test_name="batch_send_inline"
  rtt_test_name="rtt_latency_inline"
  send_pass_client="PERF_SEND_LATENCY_INLINE_CLIENT_PASS"
  batch_pass_client="PERF_BATCH_SEND_INLINE_CLIENT_PASS"
  selective_pass_client="PERF_BATCH_SEND_INLINE_SELECTIVE_CLIENT_PASS"
  rtt_pass_client="PERF_RTT_LATENCY_INLINE_CLIENT_PASS"
else
  send_test_name="send_latency"
  batch_test_name="batch_send"
  rtt_test_name="rtt_latency"
  send_pass_client="PERF_SEND_LATENCY_CLIENT_PASS"
  batch_pass_client="PERF_BATCH_SEND_CLIENT_PASS"
  selective_pass_client="PERF_BATCH_SEND_SELECTIVE_CLIENT_PASS"
  rtt_pass_client="PERF_RTT_LATENCY_CLIENT_PASS"
fi

if [[ "${PERF_SIGNAL_INTERVAL}" != "1" ]]; then
  if [[ "${PERF_USE_INLINE}" == "1" ]]; then
    batch_test_name="batch_send_inline_selective"
  else
    batch_test_name="batch_send_selective"
  fi
fi

echo "script_config name=dual_perf_client server=${SERVER_IP} port=${TCP_PORT} device=${RDMA_DEVICE} ib_port=${RDMA_IB_PORT} netdev=${RDMA_NETDEV} gid_index=${RDMA_GID_INDEX} iterations=${PERF_ITERATIONS} batch_size=${PERF_BATCH_SIZE} inline=${PERF_USE_INLINE} signal_interval=${PERF_SIGNAL_INTERVAL} poll_budget=${PERF_POLL_CQ_BUDGET} enable_rtt=${PERF_ENABLE_RTT} client_cpuset=${PERF_CLIENT_CPUSET:-auto} client_numa=${PERF_CLIENT_NUMA_NODE:-auto}"

run_sudo() {
  if [[ -n "${SUDO_PASSWORD:-}" ]]; then
    printf '%s\n' "${SUDO_PASSWORD}" | sudo -S "$@"
  else
    sudo "$@"
  fi
}

prepare_rxe() {
  # client 侧同样绑定到 192.168.65.x 所在网卡，复用 IPv4-mapped GID[1]。
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
perf_print_binding client
perf_make_launcher client
echo "script_case=dual_perf_client status=start log=${CLIENT_LOG}"
PERF_ITERATIONS="${PERF_ITERATIONS}" PERF_BATCH_SIZE="${PERF_BATCH_SIZE}" PERF_USE_INLINE="${PERF_USE_INLINE}" PERF_SIGNAL_INTERVAL="${PERF_SIGNAL_INTERVAL}" PERF_POLL_CQ_BUDGET="${PERF_POLL_CQ_BUDGET}" PERF_ENABLE_RTT="${PERF_ENABLE_RTT}" \
  "${PERF_LAUNCH_CMD[@]}" ./build/rdma-perf-client \
  --server "${SERVER_IP}" \
  --port "${TCP_PORT}" \
  --device "${RDMA_DEVICE}" \
  --ib-port "${RDMA_IB_PORT}" \
  --gid-index "${RDMA_GID_INDEX}" \
  > "${CLIENT_LOG}" 2>&1

for marker in \
  TCP_CONTROL_PLANE_PASS \
  RC_QP_RTS_PASS \
  "${send_pass_client}" \
  "${batch_pass_client}" \
  'cleanup=complete result=pass'; do
  grep -q "${marker}" "${CLIENT_LOG}"
done
grep -q 'perf_binding role=client' "${CLIENT_LOG}"

grep -q "perf_result test=${send_test_name}" "${CLIENT_LOG}"
grep -q "perf_result test=${batch_test_name}" "${CLIENT_LOG}"
grep -q 'perf_compare single_vs_batch' "${CLIENT_LOG}"
grep -q "poll_budget=${PERF_POLL_CQ_BUDGET}" "${CLIENT_LOG}"
grep -Eq "perf_throughput test=${batch_test_name} .*msg_per_sec=[1-9][0-9]*" "${CLIENT_LOG}"

if [[ "${PERF_SIGNAL_INTERVAL}" != "1" ]]; then
  grep -q "${selective_pass_client}" "${CLIENT_LOG}"
fi

if [[ "${PERF_ENABLE_RTT}" == "1" ]]; then
  grep -q "${rtt_pass_client}" "${CLIENT_LOG}"
  grep -q "perf_result test=${rtt_test_name}" "${CLIENT_LOG}"
  grep -q 'perf_compare send_vs_rtt' "${CLIENT_LOG}"
fi

echo "PASS: dual-machine client side perf path"
echo "script_case=dual_perf_client status=pass"
