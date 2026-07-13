#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

RDMA_DEVICE="${RDMA_DEVICE:-rxe0}"
RDMA_IB_PORT="${RDMA_IB_PORT:-1}"
RDMA_NETDEV="${RDMA_NETDEV:-ens34}"
RDMA_GID_ADDR="${RDMA_GID_ADDR:-fe80::34}"
RDMA_GID_INDEX="${RDMA_GID_INDEX:-1}"
PERF_SERVER_CPUSET="${PERF_SERVER_CPUSET:-}"
PERF_CLIENT_CPUSET="${PERF_CLIENT_CPUSET:-}"
PERF_SERVER_NUMA_NODE="${PERF_SERVER_NUMA_NODE:-}"
PERF_CLIENT_NUMA_NODE="${PERF_CLIENT_NUMA_NODE:-}"

echo "env_config device=${RDMA_DEVICE} netdev=${RDMA_NETDEV} gid_addr=${RDMA_GID_ADDR} gid_index=${RDMA_GID_INDEX} ib_port=${RDMA_IB_PORT}"
echo "env_binding server_cpuset=${PERF_SERVER_CPUSET:-auto} client_cpuset=${PERF_CLIENT_CPUSET:-auto} server_numa=${PERF_SERVER_NUMA_NODE:-auto} client_numa=${PERF_CLIENT_NUMA_NODE:-auto}"

echo "env_step=rdma_link"
rdma link show || true

echo "env_step=ipv6_addr"
ip -6 addr show "${RDMA_NETDEV}" || true

echo "env_step=ibv_devinfo"
ibv_devinfo -d "${RDMA_DEVICE}" -v | sed -n '/hca_id:/,/phys_port_cnt:/p' || true

echo "env_step=gid_window"
ibv_devinfo -d "${RDMA_DEVICE}" -v | sed -n "/port: ${RDMA_IB_PORT}/,/GID\\[$((RDMA_GID_INDEX + 2))\\]/p" | head -40 || true

echo "env_step=lscpu_summary"
lscpu | sed -n '1,20p' || true

echo "env_step=cpu_node_map"
lscpu -e=CPU,NODE,SOCKET | sed -n '1,32p' || true

echo "env_step=numactl_hardware"
if command -v numactl >/dev/null 2>&1; then
  numactl --hardware || true
else
  echo "numactl_missing"
fi
