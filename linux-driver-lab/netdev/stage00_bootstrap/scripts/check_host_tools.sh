#!/usr/bin/env bash
set -euo pipefail
mkdir -p output
OUT=output/host_tools.txt
: > "$OUT"
for tool in bash make gcc ip ethtool perf; do
  if command -v "$tool" >/dev/null 2>&1; then
    echo "OK  $tool -> $(command -v $tool)" >> "$OUT"
  else
    echo "MISS $tool" >> "$OUT"
  fi
done

echo "[stage00] host tool report written to $OUT"
