#!/usr/bin/env bash
# 脚本: 01_build_app.sh
# 功能: 使用 meson + ninja 构建 fastpath-lite 可执行文件
# 用法: ./scripts/01_build_app.sh

set -euo pipefail
# 加载公共函数库（定义 APP_DIR、APP_BIN、print_project_env 等）
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

# 创建/获取记录目录（records/YYYYMMDD_HHMMSS-user-space-fastpath/）
RECORD_DIR="$(ensure_record_dir)"
# 初始化记录文件
init_record_files "${RECORD_DIR}"
# 编译输出重定向到 BUILD.log
OUT="${RECORD_DIR}/BUILD.log"
: > "${OUT}"
# 记录本脚本执行命令到 COMMANDS.md
append_command_log "${RECORD_DIR}" "$0"

# 开始记录构建过程
{
    echo "# BUILD"
    echo
    echo "## project env"
    # 打印项目环境变量（如 APP_DIR、APP_BIN、DPDK 相关路径）
    print_project_env
    echo
    echo "## dependency check"
    # 检查 meson 构建工具是否存在
    command -v meson
    # 检查 ninja 编译工具是否存在
    command -v ninja
    # 检查 pkg-config 工具是否存在（用于检测 libdpdk）
    command -v pkg-config
    # 检查 libdpdk 是否安装，并显示版本
    pkg-config --modversion libdpdk
    echo
    echo "## make"
    # 执行 make rebuild：清理后重新配置+编译
    # make -C <dir> 表示进入指定目录执行 make
    # rebuild 是 Makefile 中的目标：clean + all
    make -C "${APP_DIR}" rebuild
    echo
    echo "## binary"
    # 查看生成的可执行文件信息
    ls -lh "${APP_BIN}"
    # file 命令显示文件类型（应该是 "ELF 64-bit executable"）
    file "${APP_BIN}" || true
} >> "${OUT}" 2>&1

echo "[OK] build saved: ${OUT}"
