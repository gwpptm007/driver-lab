#!/usr/bin/env bash
set -euo pipefail

track_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "${track_root}/tests/check_fundamentals.py"

