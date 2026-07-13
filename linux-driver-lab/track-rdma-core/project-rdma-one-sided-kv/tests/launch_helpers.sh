#!/usr/bin/env bash

rdma_role_cpuset() {
  local role="${1:?role required}"

  case "${role}" in
    server) printf '%s' "${RDMA_SERVER_CPUSET:-}" ;;
    client) printf '%s' "${RDMA_CLIENT_CPUSET:-}" ;;
    *) return 1 ;;
  esac
}

rdma_role_numa_node() {
  local role="${1:?role required}"

  case "${role}" in
    server) printf '%s' "${RDMA_SERVER_NUMA_NODE:-}" ;;
    client) printf '%s' "${RDMA_CLIENT_NUMA_NODE:-}" ;;
    *) return 1 ;;
  esac
}

rdma_print_binding() {
  local role="${1:?role required}"
  local cpuset
  local numa_node

  cpuset="$(rdma_role_cpuset "${role}")"
  numa_node="$(rdma_role_numa_node "${role}")"
  echo "script_binding role=${role} requested_cpuset=${cpuset:-auto} requested_numa_node=${numa_node:-auto}"
}

rdma_make_launcher() {
  local role="${1:?role required}"
  local cpuset
  local numa_node

  RDMA_LAUNCH_CMD=()
  cpuset="$(rdma_role_cpuset "${role}")"
  numa_node="$(rdma_role_numa_node "${role}")"

  # 先处理 NUMA，再处理 taskset。
  # 这样日志里的 cpus_allowed/mems_allowed 更容易和启动参数对上。
  if [[ -n "${numa_node}" ]]; then
    if ! command -v numactl >/dev/null 2>&1; then
      echo "missing_tool role=${role} tool=numactl requested_numa_node=${numa_node}" >&2
      return 1
    fi
    RDMA_LAUNCH_CMD+=(numactl "--cpunodebind=${numa_node}" "--membind=${numa_node}")
  fi

  if [[ -n "${cpuset}" ]]; then
    if ! command -v taskset >/dev/null 2>&1; then
      echo "missing_tool role=${role} tool=taskset requested_cpuset=${cpuset}" >&2
      return 1
    fi
    RDMA_LAUNCH_CMD+=(taskset -c "${cpuset}")
  fi
}
