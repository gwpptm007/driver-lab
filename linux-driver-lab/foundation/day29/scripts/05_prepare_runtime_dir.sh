#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

rd="$(run_dir)"
ensure_dir "$rd"
: > "$rd/qemu.stderr.log"
: > "$rd/serial.log"
: > "$rd/qemu-command.txt"
echo '[day29] 运行目录已准备好：'"$rd"
