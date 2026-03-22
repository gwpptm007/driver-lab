#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 运行前准备 workdir/runs/<RUN_ID>/ 目录，并创建 EDU 后端文件。
# Day27 用的是单 VM 的 EDU，不需要 peer VM 或 ivshmem-server，只要给 QEMU 挂 EDU 设备即可。

rd="$(run_dir)"
ensure_dir "$rd"
: > "$rd/qemu.stderr.log"
: > "$rd/serial.log"
: > "$rd/qemu-command.txt"
echo '[day27] 运行目录已准备好：'"$rd"
