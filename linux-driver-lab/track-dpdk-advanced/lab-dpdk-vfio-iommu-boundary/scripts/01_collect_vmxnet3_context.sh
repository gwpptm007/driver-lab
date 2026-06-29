#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
OUT="${RECORD_DIR}/VMXNET3_CONTEXT.log"
{
  echo "# VMXNET3_CONTEXT"; echo; log_env; echo
  echo "## ip link"; ip -br link || true; echo
  echo "## ip addr"; ip -br addr || true; echo
  echo "## lspci selected"; lspci -nnk -s "$DPDK_PCI" || true; echo; lspci -nnk -s "$MGMT_PCI" || true; echo
  echo "## ethtool driver"; ethtool -i "$DPDK_IF" 2>/dev/null || true; echo; ethtool -i "$MGMT_IF" 2>/dev/null || true; echo
  echo "## interrupts"; grep -E "$DPDK_IF|$MGMT_IF|vmxnet3" /proc/interrupts || true; echo
  echo "## prior evidence references"
  echo "track-dpdk basic: vmxnet3 PMD TX path exists in prior records"
  echo "network-data-plane-v1 boundary: vmxnet3 RX blocked under VMware + UIO when MSI-X/interrupt path is insufficient"
} 2>&1 | tee "$OUT"
echo "[OK] vmxnet3 context saved: $OUT"
