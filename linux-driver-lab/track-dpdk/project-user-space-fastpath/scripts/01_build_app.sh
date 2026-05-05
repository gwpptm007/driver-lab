#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/BUILD.log"
: > "${OUT}"
append_command_log "${RECORD_DIR}" "$0"

{
    echo "# BUILD"
    echo
    echo "## project env"
    print_project_env
    echo
    echo "## dependency check"
    command -v meson
    command -v ninja
    command -v pkg-config
    pkg-config --modversion libdpdk
    echo
    echo "## make"
    make -C "${APP_DIR}" rebuild
    echo
    echo "## binary"
    ls -lh "${APP_BIN}"
    file "${APP_BIN}" || true
} >> "${OUT}" 2>&1

echo "[OK] build saved: ${OUT}"
