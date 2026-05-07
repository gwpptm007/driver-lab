#!/usr/bin/env bash
#===============================================================================
# 06_run_rewrite_test.sh - MAC  rewrite 测试
# 作用：FASTPATH_REWRITE_ENABLE=1 运行 fastpath-lite，验证 rewrite 计数
#      实际调用 03_run_fastpath_rx.sh
# 输出：records/<tag>/FASTPATH_RX.log
#===============================================================================
source "$(dirname "$0")/common.sh"

export FASTPATH_UDP_ONLY=1
export FASTPATH_REWRITE_ENABLE=1
export FASTPATH_EXTRA_APP_ARGS="${FASTPATH_EXTRA_APP_ARGS}"
export RECORD_DIR="${RECORD_DIR}"
"${PROJECT_ROOT}/scripts/03_run_fastpath_rx.sh"
