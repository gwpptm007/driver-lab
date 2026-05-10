#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/BUILD.log"

{
    write_env_header
    echo
    echo "== make show =="
    make -C "${APP_DIR}" show
    echo
    echo "== clean =="
    make -C "${APP_DIR}" clean
    echo
    echo "== build =="
    make -C "${APP_DIR}" all
    echo
    echo "== artifacts =="
    ls -lh "${APP_DIR}/build" || true
    file "${APP_DIR}/build/xdp_redirect_basics.bpf.o" "${APP_DIR}/build/xdp_loader" || true
    echo
    echo "BUILD_RESULT=PASS"
} 2>&1 | tee "${OUT}"
