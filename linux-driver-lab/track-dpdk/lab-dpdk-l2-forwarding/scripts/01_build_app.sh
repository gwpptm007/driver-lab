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
    echo "## env"
    print_lab_env
    echo
    echo "## toolchain"
    gcc --version | head -n 1 || true
    meson --version || true
    ninja --version || true
    pkg-config --modversion libdpdk || true
    echo
    echo "## meson setup"
    cd "${APP_DIR}"
    if [[ ! -d "${BUILD_DIR}" ]]; then
        meson setup "${BUILD_DIR}"
    else
        meson setup "${BUILD_DIR}" --reconfigure
    fi
    echo
    echo "## ninja"
    ninja -C "${BUILD_DIR}"
    echo
    echo "## binary"
    ls -lah "${APP_BIN}"
    file "${APP_BIN}" || true
} >> "${OUT}" 2>&1

echo "[OK] build saved: ${OUT}"
echo "[OK] binary: ${APP_BIN}"
