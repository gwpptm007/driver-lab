#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
python3 "$SCRIPT_DIR/summarize_day20_records.py" >/dev/null
python3 "$SCRIPT_DIR/verify_day20_suite.py" "$@"
