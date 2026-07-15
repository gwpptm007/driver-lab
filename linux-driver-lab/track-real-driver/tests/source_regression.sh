#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

find_kernel_src() {
  local candidate
  for candidate in \
    "${KERNEL_SRC:-}" \
    "${root}/../../kernel-src/linux-5.15.10" \
    "${root}/../../kernel-src/linux-5.15.10/src" \
    "${root}/../../../kernel-src/linux-5.15.10" \
    "${root}/../../../kernel-src/linux-5.15.10/src" \
    "/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10" \
    "/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src"; do
    # 必须同时具备两个目标驱动，避免命中只保存单文件副本的目录。
    if [[ -n "${candidate}" \
      && -f "${candidate}/drivers/net/virtio_net.c" \
      && -f "${candidate}/drivers/net/ethernet/intel/e1000e/netdev.c" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done
  return 1
}

kernel_src=$(find_kernel_src || true)
if [[ -z "${kernel_src}" ]]; then
  echo "REAL_DRIVER_SOURCE_REGRESSION_SKIP reason=kernel_source_not_found"
  exit 0
fi

virtio_file="${kernel_src}/drivers/net/virtio_net.c"
e1000e_file="${kernel_src}/drivers/net/ethernet/intel/e1000e/netdev.c"

check_symbol() {
  local file="$1"
  local symbol="$2"
  if ! grep -Eq "${symbol}" "${file}"; then
    echo "REAL_DRIVER_SOURCE_REGRESSION_FAIL symbol=${symbol} file=${file}"
    exit 1
  fi
  echo "REAL_DRIVER_SOURCE_SYMBOL_PASS symbol=${symbol}"
}

# 只检查跨版本相对稳定的核心符号，避免用固定行号绑定单一内核版本。
check_symbol "${virtio_file}" 'virtnet_probe'
check_symbol "${virtio_file}" 'virtnet_poll'
check_symbol "${virtio_file}" 'start_xmit'
check_symbol "${e1000e_file}" 'e1000_probe'
check_symbol "${e1000e_file}" 'e1000e_poll'
check_symbol "${e1000e_file}" 'e1000_clean_rx_irq'

echo "REAL_DRIVER_SOURCE_REGRESSION_PASS kernel=${kernel_src}"
