#!/usr/bin/env bash
set -euo pipefail

TRACK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "${TRACK_ROOT}"

# 文档审计不编译应用，也不修改 hugepage、PCI driver 或网络配置。
python3 tests/check_docs.py
