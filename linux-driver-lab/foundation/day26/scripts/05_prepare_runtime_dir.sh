#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# Day26 使用 QEMU EDU 设备，不需要像 ivshmem 那样准备额外共享内存后端。
# 这里主要作用是统一创建 workdir/runs/<RUN_ID>/，给后续 qemu 日志和 records 提取准备目录。
rd="$(run_dir)"
ensure_dir "$rd"
echo "[day26] runtime 目录已准备：$rd"
