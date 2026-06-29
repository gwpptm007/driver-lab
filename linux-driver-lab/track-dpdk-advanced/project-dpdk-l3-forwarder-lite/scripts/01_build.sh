#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
OUT="${RECORD_DIR}/BUILD.log"
{
  echo "# BUILD"; echo; log_env; echo
  cd "$APP_DIR"; make clean; make all; ls -lh "$APP_BIN"; file "$APP_BIN" || true
} 2>&1 | tee "$OUT"
echo "[OK] build saved: $OUT"

