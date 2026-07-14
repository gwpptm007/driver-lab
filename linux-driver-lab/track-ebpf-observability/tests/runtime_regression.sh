#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
duration="${EBPF_TEST_DURATION:-3}"

if [[ "${EUID}" -ne 0 ]]; then
  echo "EBPF_RUNTIME_FAIL reason=root_required" >&2
  exit 1
fi

generate_traffic() {
  for _ in $(seq 1 80); do
    ping -c 1 -W 0.02 127.0.0.1 >/dev/null 2>&1 || true
    sleep 0.02
  done
}

run_with_traffic() {
  local name="$1"
  local work_dir="$2"
  shift 2
  echo "EBPF_RUNTIME_CASE_BEGIN name=${name}"
  generate_traffic &
  local traffic_pid=$!
  # observer 会按相对路径加载 build/*.bpf.o，必须从所属项目目录启动。
  (cd "${work_dir}" && "$@")
  wait "${traffic_pid}" || true
  echo "EBPF_RUNTIME_CASE_PASS name=${name}"
}

"${root}/tests/software_regression.sh"

# tracepoint smoke 只聚合 loopback TX 计数，不修改任何网络配置。
generate_traffic &
traffic_pid=$!
timeout "$((duration + 2))" bpftrace -e "tracepoint:net:net_dev_queue { @tx = count(); } interval:s:${duration} { print(@tx); exit(); }"
wait "${traffic_pid}" || true
echo "EBPF_RUNTIME_CASE_PASS name=bpftrace_net_dev_queue"

run_with_traffic libbpf_skb_observer \
  "${root}/lab-libbpf-net-observer" \
  ./build/skb_observer -v -d "${duration}"
run_with_traffic project_net_observer \
  "${root}/project-linux-network-observability" \
  ./build/net_observer -v -d "${duration}"

echo "EBPF_OBSERVABILITY_RUNTIME_REGRESSION_PASS"
