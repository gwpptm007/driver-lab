#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

OUT="${RECORD_DIR}/ENV_CHECK.log"

{
  echo "# ENV_CHECK"
  echo
  log_env
  echo
  echo "## tools"
  for cmd in cc make pkg-config python3; do
    if command -v "$cmd" >/dev/null 2>&1; then
      echo "OK $cmd $(command -v "$cmd")"
    else
      echo "MISS $cmd"
    fi
  done
  echo
  echo "## dpdk pkg-config"
  if pkg-config --exists libdpdk; then
    echo "OK libdpdk"
    pkg-config --modversion libdpdk || true
    pkg-config --cflags libdpdk || true
  else
    echo "MISS libdpdk"
  fi
  echo
  echo "## hugepages"
  grep -H . /sys/kernel/mm/hugepages/hugepages-*/nr_hugepages 2>/dev/null || true
} 2>&1 | tee "${OUT}"

echo "[OK] env check saved: ${OUT}"
