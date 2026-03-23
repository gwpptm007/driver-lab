#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
ensure_day35_dirs

missing=0
for d in 29 30 31 32 33 34; do
    if ! find "${LAB_ROOT}/day${d}/records" -mindepth 1 -maxdepth 1 -type d | grep -q .; then
        echo "[day35][ERROR] day${d}/records 下未发现有效运行目录"
        missing=1
    else
        echo "[day35][OK] day${d}/records 已发现运行目录"
    fi
done

if [ "$missing" -ne 0 ]; then
    exit 1
fi
