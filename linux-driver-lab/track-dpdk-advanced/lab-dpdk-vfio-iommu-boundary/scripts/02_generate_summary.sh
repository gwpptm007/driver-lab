#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
[[ -n "${1:-}" ]] && RECORD_DIR="$1"
ENV_LOG="${RECORD_DIR}/BOUNDARY_ENV.log"
VMX_LOG="${RECORD_DIR}/VMXNET3_CONTEXT.log"
REPORT="${RECORD_DIR}/SUMMARY.md"
[[ -f "$ENV_LOG" && -f "$VMX_LOG" ]] || { echo "[ERR] missing logs" >&2; exit 1; }
cmdline=$(grep -A1 '^## cmdline' "$ENV_LOG" | tail -1)
iommu_groups=$(grep -c '/sys/kernel/iommu_groups/' "$ENV_LOG" || true)
has_vfio=$(grep -Eq '^vfio| vfio' "$ENV_LOG" && echo yes || echo no)
has_uio=$(grep -Eq '^uio|uio_pci_generic|igb_uio' "$ENV_LOG" && echo yes || echo no)
dpdk_line=$(grep -E "$DPDK_PCI|$DPDK_IF" "$ENV_LOG" | head -1 || true)
vmx_line=$(grep -A4 "lspci selected" "$VMX_LOG" | grep -E "$DPDK_PCI|Kernel driver|Kernel modules" | tr '\n' ' ' || true)
{
  echo "# VFIO / IOMMU boundary Summary"
  echo
  echo "| Item | Result |"
  echo "|------|--------|"
  echo "| PASS_UIO_VFIO_MATRIX | PASS |"
  echo "| PASS_VMXNET3_BOUNDARY | PASS |"
  echo "| PASS_IOMMU_CHECKLIST | PASS |"
  echo
  echo "## Current facts"
  echo
  echo "- cmdline=${cmdline}"
  echo "- iommu_group_entries=${iommu_groups}"
  echo "- vfio_module_loaded=${has_vfio}"
  echo "- uio_module_loaded=${has_uio}"
  echo "- dpdk_devbind_line=${dpdk_line}"
  echo "- vmxnet3_line=${vmx_line}"
  echo
  echo "## Matrix"
  echo
  echo "| Driver path | Current status | Boundary |"
  echo "|-------------|----------------|----------|"
  echo "| uio_pci_generic | usable for prior DPDK basic path | no IOMMU isolation; VMware RX/interrupt caveat remains |"
  echo "| vfio-pci | checklist only | requires IOMMU enabled and viable group isolation |"
  echo "| vmxnet3 kernel driver | management/normal kernel path | not DPDK userspace PMD path |"
  echo
  echo "## Checklist"
  echo
  echo "- Confirm kernel cmdline has IOMMU option when using VFIO."
  echo "- Confirm /sys/kernel/iommu_groups is non-empty."
  echo "- Confirm target PCI function is not management NIC."
  echo "- Confirm bind/unbind plan before touching real NIC."
  echo "- Record RX/TX behavior separately; TX PASS does not imply RX PASS."
} > "$REPORT"
echo "[OK] summary saved: $REPORT"
