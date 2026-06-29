#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
: "${DPDK_PCI:=0000:0b:00.0}"
: "${DPDK_IF:=ens192}"
: "${MGMT_IF:=ens33}"
: "${MGMT_PCI:=0000:02:01.0}"
if [[ -z "${RECORD_DIR:-}" ]]; then
  RECORD_DIR="${PROJECT_ROOT}/records/$(date +%Y%m%d-%H%M%S)-vfio-iommu"
fi
mkdir -p "$RECORD_DIR"
find_dpdk_devbind() {
  for p in /usr/share/dpdk/usertools/dpdk-devbind.py /usr/local/share/dpdk/usertools/dpdk-devbind.py dpdk-devbind.py; do
    if [[ -x "$p" ]] || command -v "$p" >/dev/null 2>&1; then echo "$p"; return 0; fi
  done
  return 1
}
log_env() {
  cat <<EOF
PROJECT_ROOT=${PROJECT_ROOT}
RECORD_DIR=${RECORD_DIR}
DPDK_PCI=${DPDK_PCI}
DPDK_IF=${DPDK_IF}
MGMT_PCI=${MGMT_PCI}
MGMT_IF=${MGMT_IF}
EOF
}
