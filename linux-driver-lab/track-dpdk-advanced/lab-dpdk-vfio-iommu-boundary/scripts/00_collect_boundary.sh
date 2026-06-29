#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
OUT="${RECORD_DIR}/BOUNDARY_ENV.log"
{
  echo "# BOUNDARY_ENV"; echo; log_env; echo
  echo "## uname"; uname -a || true; echo
  echo "## cmdline"; cat /proc/cmdline || true; echo
  echo "## cpu virtualization flags"; lscpu | egrep 'Virtualization|Vendor|Model name|CPU\(s\)|NUMA' || true; echo
  echo "## iommu sysfs"; find /sys/kernel/iommu_groups -maxdepth 2 -type l 2>/dev/null | head -100 || true; echo
  echo "## modules"; lsmod | egrep 'vfio|uio|igb_uio|vmxnet3' || true; echo
  echo "## hugepages"; grep -H . /sys/kernel/mm/hugepages/hugepages-*/nr_hugepages 2>/dev/null || true; echo
  echo "## dpdk-devbind"; if devbind=$(find_dpdk_devbind); then "$devbind" --status || true; else echo "MISS dpdk-devbind.py"; fi
} 2>&1 | tee "$OUT"
echo "[OK] boundary env saved: $OUT"
