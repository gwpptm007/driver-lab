#!/usr/bin/env bash
#============================================================
# 01_build_app.sh — 编译 AF_XDP 应用
#
# 功能：
#   在 app/ 目录下执行 make，编译：
#     - af_xdp_kern.bpf.o（BPF 内核侧目标文件）
#     - af_xdp_rings（用户态可执行文件）
#
# 输出：
#   - BUILD.log（编译完整输出）
#   - build/af_xdp_kern.bpf.o
#   - build/af_xdp_rings
#
# 前置条件：
#   - clang / make / libbpf-dev / libelf-dev / zlib1g-dev 已安装
#   - （头文件检查在 00_check_env.sh 中完成）
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

REC_DIR="$(latest_record_dir)"
OUT="${REC_DIR}/BUILD.log"

{
    # 输出当前环境变量（调试用）
    write_env_header
    echo

    # 显示 Makefile 中的变量定义（确认编译选项）
    echo "== make show =="
    make -C "${APP_DIR}" show
    echo

    # 执行清理 + 编译
    echo "== build =="
    make -C "${APP_DIR}" clean all
    echo

    # 验证生成的文件
    echo "== artifacts =="
    file "${APP_DIR}/build/af_xdp_kern.bpf.o" || true
    file "${APP_DIR}/build/af_xdp_rings" || true
    echo

    echo "BUILD_RESULT=PASS"
} 2>&1 | tee "${OUT}"