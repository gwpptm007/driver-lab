#!/usr/bin/env bash
# =============================================================================
# 01_build_app.sh — 编译 l2fwd-lite（不需要 root）
#
# 流程：
#   1. 打印工具链版本（gcc, meson, ninja, libdpdk）
#   2. meson setup 初始化/重新配置编译目录
#   3. ninja 编译生成 app/build/l2fwd-lite
#   4. file 命令验证产物格式
#
# 产出：records/<timestamp>-dpdk-l2-forwarding/BUILD.log
# =============================================================================
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 复用已有的记录目录（如果存在）
RECORD_DIR="$(ensure_record_dir)"
init_record_files "${RECORD_DIR}"
OUT="${RECORD_DIR}/BUILD.log"
: > "${OUT}"

append_command_log "${RECORD_DIR}" "$0"

{
    echo "# BUILD"
    echo
    # ── 打印环境变量 ─────────────────────────────────────────
    echo "## env"
    print_lab_env
    echo
    # ── 工具链版本 ───────────────────────────────────────────
    echo "## toolchain"
    gcc --version | head -n 1 || true
    meson --version || true
    ninja --version || true
    pkg-config --modversion libdpdk || true
    echo
    # ── meson setup ──────────────────────────────────────────
    # 首次编译：meson setup build
    # 后续编译：meson setup build --reconfigure（更新配置）
    echo "## meson setup"
    cd "${APP_DIR}"
    if [[ ! -d "${BUILD_DIR}" ]]; then
        meson setup "${BUILD_DIR}"
    else
        meson setup "${BUILD_DIR}" --reconfigure
    fi
    echo
    # ── ninja 编译 ───────────────────────────────────────────
    echo "## ninja"
    ninja -C "${BUILD_DIR}"
    echo
    # ── 验证编译产物 ─────────────────────────────────────────
    echo "## binary"
    ls -lah "${APP_BIN}"
    file "${APP_BIN}" || true
} >> "${OUT}" 2>&1

echo "[OK] build saved: ${OUT}"
echo "[OK] binary: ${APP_BIN}"
