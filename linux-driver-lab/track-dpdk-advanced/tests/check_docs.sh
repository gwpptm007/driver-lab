#!/usr/bin/env bash
set -euo pipefail

TRACK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "${TRACK_ROOT}"

# 仅检查文档，不修改 PCI、hugepage 或网络配置。
python3 tests/check_docs.py
