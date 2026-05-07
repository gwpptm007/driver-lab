#!/usr/bin/env bash
#===============================================================================
# 05_run_udp_only_test.sh - UDP-only 过滤测试
# 作用：FASTPATH_UDP_ONLY=1 运行 fastpath-lite，验证非 UDP 包被 drop
#      实际调用 03_run_fastpath_rx.sh
# 输出：records/<tag>/FASTPATH_RX.log
#===============================================================================
source "$(dirname "$0")/common.sh"

export FASTPATH_UDP_ONLY=1
export FASTPATH_REWRITE_ENABLE=0
export RECORD_DIR="${RECORD_DIR}"
"${PROJECT_ROOT}/scripts/03_run_fastpath_rx.sh"
