#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"

# 始终清理后重编译，避免旧对象掩盖头文件或编译参数变化。
cd "${APP_DIR}"
make clean
make
echo "FLOW_BUILD_PASS binary=${APP_BIN}"
