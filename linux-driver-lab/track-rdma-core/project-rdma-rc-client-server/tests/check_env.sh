#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
source tests/launch_helpers.sh

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_IB_PORT="${RDMA_IB_PORT:-1}"
RDMA_NETDEV="${RDMA_NETDEV:-ens34}"
RDMA_GID_INDEX="${RDMA_GID_INDEX:-1}"

echo "script_config name=check_env device=${RDMA_DEVICE} ib_port=${RDMA_IB_PORT} netdev=${RDMA_NETDEV} gid_index=${RDMA_GID_INDEX}"
rdma_print_binding server
rdma_print_binding client

echo "script_step=rdma_link status=start"
rdma link show
echo "script_step=rdma_link status=done"

echo "script_step=ip_addr status=start netdev=${RDMA_NETDEV}"
ip -6 addr show "${RDMA_NETDEV}"
echo "script_step=ip_addr status=done netdev=${RDMA_NETDEV}"

echo "script_step=ibv_devinfo status=start device=${RDMA_DEVICE}"
ibv_devinfo -d "${RDMA_DEVICE}" -v | sed -n '/GID\[/,+2p' | head -20
echo "script_step=ibv_devinfo status=done device=${RDMA_DEVICE}"

echo "script_step=lscpu status=start"
lscpu
echo "script_step=lscpu status=done"

echo "script_step=lscpu_topology status=start"
lscpu -e=CPU,NODE,SOCKET
echo "script_step=lscpu_topology status=done"

if command -v numactl >/dev/null 2>&1; then
  echo "script_step=numactl status=start"
  numactl --hardware
  echo "script_step=numactl status=done"
else
  echo "numactl_missing=1"
fi
