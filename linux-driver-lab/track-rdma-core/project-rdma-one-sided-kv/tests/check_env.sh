#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
source tests/launch_helpers.sh

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_NETDEV="${RDMA_NETDEV:-ens34}"

echo "script_config name=check_env device=${RDMA_DEVICE} netdev=${RDMA_NETDEV}"
rdma_print_binding server
rdma_print_binding client
echo 'script_step=rdma_link status=start'
rdma link show
echo 'script_step=rdma_link status=done'
echo "script_step=ibv_devinfo status=start device=${RDMA_DEVICE}"
ibv_devinfo -d "${RDMA_DEVICE}" -v | sed -n '/GID\[/,+2p' | head -20
echo "script_step=ibv_devinfo status=done device=${RDMA_DEVICE}"
echo 'script_step=lscpu_topology status=start'
lscpu -e=CPU,NODE,SOCKET
echo 'script_step=lscpu_topology status=done'
