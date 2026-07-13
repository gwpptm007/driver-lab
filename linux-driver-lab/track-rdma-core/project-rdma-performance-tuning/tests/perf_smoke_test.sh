#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
. tests/perf_launch_helpers.sh

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_IB_PORT="${RDMA_IB_PORT:-1}"
RDMA_NETDEV="${RDMA_NETDEV:-ens34}"
RDMA_GID_ADDR="${RDMA_GID_ADDR:-fe80::34}"
RDMA_GID_INDEX="${RDMA_GID_INDEX:-1}"
PERF_SERVER_CPUSET="${PERF_SERVER_CPUSET:-}"
PERF_CLIENT_CPUSET="${PERF_CLIENT_CPUSET:-}"
PERF_SERVER_NUMA_NODE="${PERF_SERVER_NUMA_NODE:-}"
PERF_CLIENT_NUMA_NODE="${PERF_CLIENT_NUMA_NODE:-}"
PERF_ITERATIONS="${PERF_ITERATIONS:-100}"
PERF_BATCH_SIZE="${PERF_BATCH_SIZE:-8}"
PERF_USE_INLINE="${PERF_USE_INLINE:-0}"
PERF_SIGNAL_INTERVAL="${PERF_SIGNAL_INTERVAL:-1}"
PERF_POLL_CQ_BUDGET="${PERF_POLL_CQ_BUDGET:-16}"
PERF_ENABLE_RTT="${PERF_ENABLE_RTT:-0}"
PERF_SKIP_CLEAN="${PERF_SKIP_CLEAN:-0}"
TCP_PORT="${TCP_PORT:-18600}"

if [[ "${PERF_USE_INLINE}" == "1" ]]; then
  send_test_name="send_latency_inline"
  rtt_test_name="rtt_latency_inline"
  send_pass_server="PERF_SEND_LATENCY_INLINE_SERVER_PASS"
  send_pass_client="PERF_SEND_LATENCY_INLINE_CLIENT_PASS"
  batch_pass_server="PERF_BATCH_SEND_INLINE_SERVER_PASS"
  batch_pass_client="PERF_BATCH_SEND_INLINE_CLIENT_PASS"
  rtt_pass_server="PERF_RTT_LATENCY_INLINE_SERVER_PASS"
  rtt_pass_client="PERF_RTT_LATENCY_INLINE_CLIENT_PASS"
else
  send_test_name="send_latency"
  rtt_test_name="rtt_latency"
  send_pass_server="PERF_SEND_LATENCY_SERVER_PASS"
  send_pass_client="PERF_SEND_LATENCY_CLIENT_PASS"
  batch_pass_server="PERF_BATCH_SEND_SERVER_PASS"
  batch_pass_client="PERF_BATCH_SEND_CLIENT_PASS"
  rtt_pass_server="PERF_RTT_LATENCY_SERVER_PASS"
  rtt_pass_client="PERF_RTT_LATENCY_CLIENT_PASS"
fi

if [[ "${PERF_SIGNAL_INTERVAL}" == "1" ]]; then
  selective_pass_server=""
  selective_pass_client=""
  if [[ "${PERF_USE_INLINE}" == "1" ]]; then
    batch_test_name="batch_send_inline"
    script_case="send_latency_batch_wr_inline"
  else
    batch_test_name="batch_send"
    script_case="send_latency_batch_wr"
  fi
else
  if [[ "${PERF_USE_INLINE}" == "1" ]]; then
    batch_test_name="batch_send_inline_selective"
    selective_pass_server="PERF_BATCH_SEND_INLINE_SELECTIVE_SERVER_PASS"
    selective_pass_client="PERF_BATCH_SEND_INLINE_SELECTIVE_CLIENT_PASS"
    script_case="send_latency_batch_wr_inline_selective"
  else
    batch_test_name="batch_send_selective"
    selective_pass_server="PERF_BATCH_SEND_SELECTIVE_SERVER_PASS"
    selective_pass_client="PERF_BATCH_SEND_SELECTIVE_CLIENT_PASS"
    script_case="send_latency_batch_wr_selective"
  fi
fi

echo "script_config name=perf_smoke_test device=${RDMA_DEVICE} netdev=${RDMA_NETDEV} gid_addr=${RDMA_GID_ADDR} gid_index=${RDMA_GID_INDEX} iterations=${PERF_ITERATIONS} batch_size=${PERF_BATCH_SIZE} inline=${PERF_USE_INLINE} signal_interval=${PERF_SIGNAL_INTERVAL} poll_budget=${PERF_POLL_CQ_BUDGET} enable_rtt=${PERF_ENABLE_RTT} server_cpuset=${PERF_SERVER_CPUSET:-auto} client_cpuset=${PERF_CLIENT_CPUSET:-auto} server_numa=${PERF_SERVER_NUMA_NODE:-auto} client_numa=${PERF_CLIENT_NUMA_NODE:-auto}"

run_sudo() {
  if [[ -n "${SUDO_PASSWORD:-}" ]]; then
    printf '%s\n' "${SUDO_PASSWORD}" | sudo -S "$@"
  else
    sudo "$@"
  fi
}

cleanup() {
  if [[ -n "${server_pid:-}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

if [[ "${PERF_SKIP_CLEAN}" != "1" ]]; then
  make clean
fi
make

echo "script_step=prepare_rxe status=start netdev=${RDMA_NETDEV}"
run_sudo modprobe rdma_rxe >/dev/null 2>&1 || true
run_sudo ip -6 addr add "${RDMA_GID_ADDR}/64" dev "${RDMA_NETDEV}" >/dev/null 2>&1 || true
run_sudo rdma link delete "${RDMA_DEVICE}" >/dev/null 2>&1 || true
run_sudo rdma link add "${RDMA_DEVICE}" type rxe netdev "${RDMA_NETDEV}" >/dev/null 2>&1 || true
while ip -6 addr show "${RDMA_NETDEV}" | grep -q tentative; do sleep 1; done
echo "script_step=prepare_rxe status=done"

perf_print_binding server
perf_make_launcher server
echo "script_case=${script_case} status=start server_log=tests/perf-server.log client_log=tests/perf-client.log"
PERF_ITERATIONS="${PERF_ITERATIONS}" PERF_BATCH_SIZE="${PERF_BATCH_SIZE}" PERF_USE_INLINE="${PERF_USE_INLINE}" PERF_SIGNAL_INTERVAL="${PERF_SIGNAL_INTERVAL}" PERF_POLL_CQ_BUDGET="${PERF_POLL_CQ_BUDGET}" PERF_ENABLE_RTT="${PERF_ENABLE_RTT}" \
  "${PERF_LAUNCH_CMD[@]}" ./build/rdma-perf-server --listen 127.0.0.1 --port "${TCP_PORT}" \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" \
  > tests/perf-server.log 2>&1 &
server_pid=$!

sleep 1
perf_print_binding client
perf_make_launcher client
PERF_ITERATIONS="${PERF_ITERATIONS}" PERF_BATCH_SIZE="${PERF_BATCH_SIZE}" PERF_USE_INLINE="${PERF_USE_INLINE}" PERF_SIGNAL_INTERVAL="${PERF_SIGNAL_INTERVAL}" PERF_POLL_CQ_BUDGET="${PERF_POLL_CQ_BUDGET}" PERF_ENABLE_RTT="${PERF_ENABLE_RTT}" \
  "${PERF_LAUNCH_CMD[@]}" ./build/rdma-perf-client --server 127.0.0.1 --port "${TCP_PORT}" \
  --device "${RDMA_DEVICE}" --ib-port "${RDMA_IB_PORT}" --gid-index "${RDMA_GID_INDEX}" \
  > tests/perf-client.log 2>&1

wait "${server_pid}"
trap - EXIT

grep -q "${send_pass_server}" tests/perf-server.log
grep -q "${send_pass_client}" tests/perf-client.log
grep -q "perf_result test=${send_test_name}" tests/perf-client.log
grep -q "${batch_pass_server}" tests/perf-server.log
grep -q "${batch_pass_client}" tests/perf-client.log
if [[ -n "${selective_pass_server}" ]]; then
  grep -q "${selective_pass_server}" tests/perf-server.log
  grep -q "${selective_pass_client}" tests/perf-client.log
fi
grep -q "perf_result test=${batch_test_name}" tests/perf-client.log
grep -q 'perf_compare single_vs_batch' tests/perf-client.log
grep -q "signal_interval=${PERF_SIGNAL_INTERVAL}" tests/perf-client.log
grep -q "poll_budget=${PERF_POLL_CQ_BUDGET}" tests/perf-client.log
grep -q 'poll_mode=' tests/perf-client.log
grep -q 'perf_binding role=server' tests/perf-server.log
grep -q 'perf_binding role=client' tests/perf-client.log
grep -Eq "perf_result test=${send_test_name} .*avg_ns=[1-9][0-9]*" tests/perf-client.log
grep -Eq "perf_result test=${batch_test_name} .*avg_msg_ns=[1-9][0-9]*" tests/perf-client.log
grep -Eq "perf_result test=${batch_test_name} .*signaled_total=[1-9][0-9]*" tests/perf-client.log
grep -Eq "perf_throughput test=${batch_test_name} .*msg_per_sec=[1-9][0-9]*" tests/perf-client.log
if [[ "${PERF_ENABLE_RTT}" == "1" ]]; then
  grep -q "${rtt_pass_server}" tests/perf-server.log
  grep -q "${rtt_pass_client}" tests/perf-client.log
  grep -q "perf_result test=${rtt_test_name}" tests/perf-client.log
  grep -q 'perf_compare send_vs_rtt' tests/perf-client.log
  grep -Eq "perf_result test=${rtt_test_name} .*avg_ns=[1-9][0-9]*" tests/perf-client.log
fi
grep -q 'cleanup=complete result=pass' tests/perf-server.log
grep -q 'cleanup=complete result=pass' tests/perf-client.log

echo "PASS: RDMA SEND latency + batch WR smoke test inline=${PERF_USE_INLINE} signal_interval=${PERF_SIGNAL_INTERVAL} poll_budget=${PERF_POLL_CQ_BUDGET} enable_rtt=${PERF_ENABLE_RTT}"
echo "script_case=${script_case} status=pass"
