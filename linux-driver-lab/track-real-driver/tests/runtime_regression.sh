#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

"${root}/tests/software_regression.sh"
"${root}/tests/source_regression.sh"

iface=${REAL_DRIVER_IFACE:-$(ip route show default 2>/dev/null | awk 'NR == 1 {print $5}')}
target=${REAL_DRIVER_PING_TARGET:-$(ip route show default 2>/dev/null | awk 'NR == 1 {print $3}')}

if [[ -z "${iface}" || -z "${target}" || ! -d "/sys/class/net/${iface}" ]]; then
  echo "REAL_DRIVER_RUNTIME_REGRESSION_SKIP reason=route_or_interface_not_found"
  exit 0
fi

driver=$(ethtool -i "${iface}" 2>/dev/null | awk '/^driver:/ {print $2}')
case "${driver}" in
  virtio_net|e1000|e1000e) ;;
  *)
    echo "REAL_DRIVER_RUNTIME_REGRESSION_SKIP reason=non_target_driver iface=${iface} driver=${driver:-unknown}"
    exit 0
    ;;
esac

read_stat() {
  cat "/sys/class/net/${iface}/statistics/$1"
}

rx_before=$(read_stat rx_packets)
tx_before=$(read_stat tx_packets)
rx_err_before=$(read_stat rx_errors)
tx_err_before=$(read_stat tx_errors)

# 只发送少量 ICMP 流量验证真实数据路径，不修改接口、feature 或 driver 状态。
ping -I "${iface}" -c 5 -W 1 "${target}" >/dev/null

rx_after=$(read_stat rx_packets)
tx_after=$(read_stat tx_packets)
rx_err_after=$(read_stat rx_errors)
tx_err_after=$(read_stat tx_errors)

rx_delta=$((rx_after - rx_before))
tx_delta=$((tx_after - tx_before))
rx_err_delta=$((rx_err_after - rx_err_before))
tx_err_delta=$((tx_err_after - tx_err_before))

echo "REAL_DRIVER_RUNTIME_DELTA iface=${iface} driver=${driver} target=${target} rx=${rx_delta} tx=${tx_delta} rx_err=${rx_err_delta} tx_err=${tx_err_delta}"

if (( rx_delta <= 0 || tx_delta <= 0 )); then
  echo "REAL_DRIVER_RUNTIME_REGRESSION_FAIL reason=no_packet_progress"
  exit 1
fi
if (( rx_err_delta != 0 || tx_err_delta != 0 )); then
  echo "REAL_DRIVER_RUNTIME_REGRESSION_FAIL reason=error_counter_increased"
  exit 1
fi

echo "REAL_DRIVER_RUNTIME_REGRESSION_PASS iface=${iface} driver=${driver}"
