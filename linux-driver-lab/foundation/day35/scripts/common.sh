#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/../env/day35.env"

# 统一准备 Day35 自身的 output / records 目录。
ensure_day35_dirs() {
    mkdir -p "${OUTPUT_DIR}"
    mkdir -p "${RECORDS_DIR}/${RUN_ID}"
}

# 记录 Day35 脚本执行日志，便于后续复盘“这份报告是怎么生成的”。
log_to_record() {
    local target="$1"
    shift
    "$@" | tee "${RECORDS_DIR}/${RUN_ID}/${target}"
}
