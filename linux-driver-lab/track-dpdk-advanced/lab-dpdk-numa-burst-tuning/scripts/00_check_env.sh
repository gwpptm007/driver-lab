#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
OUT="${RECORD_DIR}/ENV_CHECK.log"
{
  echo "# ENV_CHECK"; echo; log_env; echo
  for cmd in cc make pkg-config python3 awk sed; do command -v "$cmd" >/dev/null && echo "OK $cmd $(command -v "$cmd")" || echo "MISS $cmd"; done
  pkg-config --exists libdpdk && { echo "OK libdpdk"; pkg-config --modversion libdpdk; } || echo "MISS libdpdk"
  grep -H . /sys/kernel/mm/hugepages/hugepages-*/nr_hugepages 2>/dev/null || true
  lscpu | egrep 'CPU\(s\)|NUMA|Model name' || true
} 2>&1 | tee "$OUT"
echo "[OK] env check saved: $OUT"
