#!/usr/bin/env bash
set -euo pipefail
NAME=${1:-manual}
TS=$(date +%Y%m%d_%H%M%S)
OUT="records/${TS}-${NAME}"
mkdir -p "$OUT"
cp docs/06_REPORT_TEMPLATE.md "$OUT/SUMMARY.md"
echo "$OUT"
