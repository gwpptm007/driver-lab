#!/usr/bin/env bash
set -euo pipefail

track_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
xdp_if="${AF_XDP_TEST_IFACE:-veth-xdp}"
peer_if="${AF_XDP_TEST_PEER:-veth-peer}"
peer_addr="${AF_XDP_TEST_PEER_ADDR:-10.99.0.2/24}"
target_addr="${AF_XDP_TEST_TARGET_ADDR:-10.99.0.1}"
duration="${AF_XDP_TEST_DURATION:-4}"

if [[ "${EUID}" -ne 0 ]]; then
  echo "AF_XDP_VETH_RUNTIME_FAIL reason=root_required" >&2
  exit 1
fi

cleanup() {
  # 只清理本脚本固定创建的测试 veth，不操作管理网口或物理设备。
  ip link set dev "${xdp_if}" xdp off >/dev/null 2>&1 || true
  ip link set dev "${xdp_if}" xdpgeneric off >/dev/null 2>&1 || true
  ip link delete "${xdp_if}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

inject_traffic() {
  sleep 0.8
  for _ in $(seq 1 80); do
    ping -c 1 -W 0.02 -I "${peer_if}" "${target_addr}" >/dev/null 2>&1 || true
    sleep 0.03
  done
}

run_case() {
  local name="$1"
  shift
  echo "AF_XDP_RUNTIME_CASE_BEGIN name=${name}"
  inject_traffic &
  local traffic_pid=$!
  "$@"
  wait "${traffic_pid}" || true
  echo "AF_XDP_RUNTIME_CASE_PASS name=${name}"
}

cleanup
ip link add "${xdp_if}" type veth peer name "${peer_if}"
ip link set "${xdp_if}" up
ip link set "${peer_if}" up
ip addr add "${peer_addr}" dev "${peer_if}"
echo "AF_XDP_VETH_READY xdp_if=${xdp_if} peer_if=${peer_if}"

"${track_root}/tests/software_regression.sh"

run_case phase1_pass env AF_XDP_IFACE="${xdp_if}" AF_XDP_MODE=skb AF_XDP_DURATION="${duration}" \
  bash "${track_root}/lab-xdp-redirect-basics/scripts/03_run_xdp_pass.sh"
run_case phase1_drop env AF_XDP_CONFIRM_DROP=YES AF_XDP_IFACE="${xdp_if}" AF_XDP_DURATION="${duration}" \
  bash "${track_root}/lab-xdp-redirect-basics/scripts/04_run_xdp_drop.sh"

run_case phase2_rx env AF_XDP_IFACE="${xdp_if}" AF_XDP_DURATION="${duration}" \
  bash "${track_root}/lab-af-xdp-socket-rings/scripts/03_run_af_xdp_socket_smoke.sh"

run_case phase3_copy env AF_XDP_IFACE="${xdp_if}" AF_XDP_DURATION="${duration}" \
  bash "${track_root}/lab-af-xdp-zero-copy-vs-copy/scripts/03_run_copy_mode_baseline.sh"
run_case phase3_native_copy env AF_XDP_IFACE="${xdp_if}" AF_XDP_DURATION="${duration}" \
  bash "${track_root}/lab-af-xdp-zero-copy-vs-copy/scripts/04_probe_native_copy.sh"
# ZC 不支持是 veth 的预期能力边界，探针脚本负责把结果分类而不是伪装成 PASS。
env AF_XDP_IFACE="${xdp_if}" AF_XDP_DURATION="${duration}" \
  bash "${track_root}/lab-af-xdp-zero-copy-vs-copy/scripts/05_probe_zero_copy.sh"
echo "AF_XDP_RUNTIME_CASE_PASS name=phase3_zero_copy_probe"

run_case phase4_drop env AF_XDP_IFACE="${xdp_if}" AF_XDP_DURATION="${duration}" \
  bash "${track_root}/project-af-xdp-mini-forwarder/scripts/03_run_forwarder_drop_smoke.sh"
run_case phase4_reflect env AF_XDP_IFACE="${xdp_if}" AF_XDP_DURATION="${duration}" \
  bash "${track_root}/project-af-xdp-mini-forwarder/scripts/04_run_forwarder_reflect_smoke.sh"

echo "AF_XDP_VETH_RUNTIME_REGRESSION_PASS"

