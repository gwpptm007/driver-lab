#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

warn "day22 当前默认使用 ivshmem-plain；此脚本仅做兼容包装。"
exec "${SCRIPT_DIR}/04_prepare_ivshmem_backend.sh"
