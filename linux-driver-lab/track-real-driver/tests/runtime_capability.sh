#!/usr/bin/env bash
set -euo pipefail

iface=${REAL_DRIVER_IFACE:-$(ip route show default 2>/dev/null | awk 'NR == 1 {print $5}')}
if [[ -z "${iface}" || ! -d "/sys/class/net/${iface}" ]]; then
  echo "REAL_DRIVER_RUNTIME_CAPABILITY_SKIP reason=default_interface_not_found"
  exit 0
fi

driver=$(ethtool -i "${iface}" 2>/dev/null | awk '/^driver:/ {print $2}')
bus=$(ethtool -i "${iface}" 2>/dev/null | awk '/^bus-info:/ {print $2}')
if [[ -z "${driver}" ]]; then
  echo "REAL_DRIVER_RUNTIME_CAPABILITY_SKIP reason=driver_identity_unavailable iface=${iface}"
  exit 0
fi

echo "REAL_DRIVER_RUNTIME_IDENTITY iface=${iface} driver=${driver} bus=${bus:-unknown}"
case "${driver}" in
  virtio_net|e1000|e1000e)
    ethtool -k "${iface}" >/dev/null
    ip -s link show dev "${iface}" >/dev/null
    echo "REAL_DRIVER_RUNTIME_CAPABILITY_PASS iface=${iface} driver=${driver}"
    ;;
  *)
    # 当前接口不是本 track 的目标驱动，只记录环境边界，不伪造运行验证。
    echo "REAL_DRIVER_RUNTIME_CAPABILITY_SKIP reason=non_target_driver iface=${iface} driver=${driver}"
    ;;
esac
