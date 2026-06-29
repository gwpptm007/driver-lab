#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
OUT="${RECORD_DIR}/ENV_CHECK.log"
{
  echo "# ENV_CHECK"; echo; log_env; echo
  uname -a
  pkg-config --modversion libdpdk
  python3 --version
  grep -H . /sys/kernel/mm/hugepages/hugepages-*/nr_hugepages 2>/dev/null || true
} 2>&1 | tee "$OUT"
echo "[OK] env saved: $OUT"

