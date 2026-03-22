#!/usr/bin/env bash
# 清理 day28 自动生成的输出。
#
# 注意：这里只清理 day28 自己生成的 output 和 snapshot，
# 不会动 day22~day27 的原始 records。

set -euo pipefail
source "$(dirname "$0")/common.sh"

rm -rf "${SNAPSHOT_DIR}"
rm -f "${OUTPUT_DIR}/day28_w4_summary.md" "${OUTPUT_DIR}/day28_evidence_index.md"
log "day28 自动生成产物已清理"
