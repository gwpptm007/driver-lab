#!/usr/bin/env bash
set -euo pipefail

rdma_device="${RDMA_DEVICE:-rxe0}"
netdev="${RDMA_NETDEV:-ens34}"
gid_addr="${RDMA_GID_ADDR:-fe80::34}"
gid_index="${RDMA_GID_INDEX:-1}"

run_sudo() {
  if [[ -n "${SUDO_PASSWORD:-}" ]]; then
    printf '%s\n' "${SUDO_PASSWORD}" | sudo -S "$@"
  else
    sudo "$@"
  fi
}

if ! ip link show dev "${netdev}" >/dev/null 2>&1; then
  echo "RDMA_RXE_PREPARE_FAIL reason=netdev_missing netdev=${netdev}" >&2
  exit 1
fi

echo "RDMA_RXE_PREPARE_BEGIN device=${rdma_device} netdev=${netdev} gid_addr=${gid_addr} gid_index=${gid_index}"
run_sudo modprobe rdma_rxe

# 使用显式测试地址生成可复现 GID；地址已存在时保持幂等。
if ! ip -6 addr show dev "${netdev}" | grep -Fq "${gid_addr}/64"; then
  run_sudo ip -6 addr add "${gid_addr}/64" dev "${netdev}"
fi

# 重建 RXE 让 provider 重新读取 netdev 地址并生成 GID 表。
if rdma link show "${rdma_device}/1" >/dev/null 2>&1; then
  run_sudo rdma link delete "${rdma_device}"
fi
run_sudo rdma link add "${rdma_device}" type rxe netdev "${netdev}"

for _ in $(seq 1 20); do
  if rdma link show "${rdma_device}/1" 2>/dev/null | grep -q 'state ACTIVE'; then
    break
  fi
  sleep 0.2
done

rdma link show "${rdma_device}/1" | grep -q 'state ACTIVE'
ibv_devinfo -d "${rdma_device}" -i 1 -v | grep -q "GID\[ *${gid_index}\]"
echo "RDMA_RXE_PREPARE_PASS device=${rdma_device} gid_index=${gid_index}"

