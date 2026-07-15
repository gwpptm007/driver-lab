#!/usr/bin/env bash

# 该文件由 Phase 3/4 测试脚本 source，不单独执行。
# 默认只验证现有环境；显式设置 GATEWAY_PREPARE_RXE=1 后才重建 RXE。

gateway_run_sudo() {
  if [[ -n "${SUDO_PASSWORD:-}" ]]; then
    printf '%s\n' "${SUDO_PASSWORD}" | sudo -S -p '' "$@"
  else
    sudo "$@"
  fi
}

gateway_prepare_rxe() {
  local device=${RDMA_DEVICE:-rxe0}
  local netdev=${GATEWAY_RDMA_NETDEV:-ens34}
  local gid_addr=${GATEWAY_RDMA_GID_ADDR:-fe80::34}

  [[ "${GATEWAY_PREPARE_RXE:-0}" == "1" ]] || return 0

  # 先固定 IPv6 link-local 地址，再重建 RXE，使目标 GID 能稳定映射到 netdev。
  gateway_run_sudo modprobe rdma_rxe
  gateway_run_sudo ip link set dev "${netdev}" up
  gateway_run_sudo ip -6 addr replace "${gid_addr}/64" dev "${netdev}" nodad
  gateway_run_sudo rdma link delete "${device}" 2>/dev/null || true
  gateway_run_sudo rdma link add "${device}" type rxe netdev "${netdev}"
}

gateway_check_rdma_env() {
  local device=${RDMA_DEVICE:-rxe0}
  local gid_index=${RDMA_GID_INDEX:-1}
  local gid_addr=${GATEWAY_RDMA_GID_ADDR:-fe80::34}
  local attempt

  gateway_prepare_rxe

  # RXE 注册及 GID 表刷新可能稍有延迟，因此用短轮询代替固定长等待。
  for attempt in $(seq 1 20); do
    if rdma link show | grep -q "${device}/1 state ACTIVE" &&
       ibv_devinfo -d "${device}" -v 2>/dev/null |
         grep -Eq "GID\[[[:space:]]*${gid_index}\].*${gid_addr}"; then
      echo "gateway_rdma_env device=${device} gid_index=${gid_index} gid=${gid_addr} status=ready"
      return 0
    fi
    sleep 0.2
  done

  echo "ERROR: RDMA environment is not ready: device=${device} gid_index=${gid_index} gid=${gid_addr}" >&2
  rdma link show >&2 || true
  ibv_devinfo -d "${device}" -v 2>/dev/null | grep -i 'GID\[' >&2 || true
  return 1
}
