#!/usr/bin/env bash
set -euo pipefail

STAMP=$(date +%Y%m%d-%H%M%S)
OUT=${1:-stage08_dmesg_${STAMP}.log}

sudo dmesg | grep -E 'stage08|netdev_stage08' | tail -n 200 | tee "$OUT"
echo "[stage08] trace log -> $OUT"
