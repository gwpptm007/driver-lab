#!/usr/bin/env bash

perf_role_cpuset() {
  local role="${1:?role required}"

  case "${role}" in
    server) printf '%s' "${PERF_SERVER_CPUSET:-}" ;;
    client) printf '%s' "${PERF_CLIENT_CPUSET:-}" ;;
    *) return 1 ;;
  esac
}

perf_role_numa_node() {
  local role="${1:?role required}"

  case "${role}" in
    server) printf '%s' "${PERF_SERVER_NUMA_NODE:-}" ;;
    client) printf '%s' "${PERF_CLIENT_NUMA_NODE:-}" ;;
    *) return 1 ;;
  esac
}

perf_print_binding() {
  local role="${1:?role required}"
  local cpuset
  local numa_node

  cpuset="$(perf_role_cpuset "${role}")"
  numa_node="$(perf_role_numa_node "${role}")"
  echo "script_binding role=${role} requested_cpuset=${cpuset:-auto} requested_numa_node=${numa_node:-auto}"
}

perf_make_launcher() {
  local role="${1:?role required}"
  local cpuset
  local numa_node

  PERF_LAUNCH_CMD=()
  cpuset="$(perf_role_cpuset "${role}")"
  numa_node="$(perf_role_numa_node "${role}")"

  if [[ -n "${numa_node}" ]]; then
    if ! command -v numactl >/dev/null 2>&1; then
      echo "missing_tool role=${role} tool=numactl requested_numa_node=${numa_node}" >&2
      return 1
    fi
    PERF_LAUNCH_CMD+=(numactl "--cpunodebind=${numa_node}" "--membind=${numa_node}")
  fi

  if [[ -n "${cpuset}" ]]; then
    if ! command -v taskset >/dev/null 2>&1; then
      echo "missing_tool role=${role} tool=taskset requested_cpuset=${cpuset}" >&2
      return 1
    fi
    PERF_LAUNCH_CMD+=(taskset -c "${cpuset}")
  fi
}
