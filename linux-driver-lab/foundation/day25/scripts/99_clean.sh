#!/usr/bin/env bash
set -euo pipefail

# 清理 workdir 和驱动目录下的编译中间产物，不删除 records。
rm -rf "${WORKDIR:-$(pwd)/workdir}"
rm -f "$(pwd)/driver"/*.o "$(pwd)/driver"/*.ko "$(pwd)/driver"/*.mod "$(pwd)/driver"/*.mod.c "$(pwd)/driver"/*.mod.o "$(pwd)/driver"/Module.symvers "$(pwd)/driver"/modules.order
find "$(pwd)/driver" -maxdepth 1 -type f -name '.*.cmd' -delete || true
