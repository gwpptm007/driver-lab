#!/usr/bin/env bash
#============================================================
# 01_build_app.sh — 编译 af_xdp_forwarder 应用
#
# 功能：
#   在 app/ 目录下执行 make，编译：
#     - af_xdp_forwarder_kern.bpf.o（BPF 内核侧目标文件）
#     - af_xdp_forwarder（用户态可执行文件）
#
# 输出：
#   - BUILD.log（编译完整输出）
#   - build/af_xdp_forwarder_kern.bpf.o
#   - build/af_xdp_forwarder
#
# 前置条件：
#   - clang / make / libbpf-dev / libelf-dev / zlib1g-dev 已安装
#   - Makefile 中已修复 -I/usr/include/x86_64-linux-gnu 路径问题
#============================================================

set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

record_dir="$(latest_record_dir)"
out="${record_dir}/BUILD.log"

{
    write_env_header
    echo

    cd "${APP_DIR}"
    make clean
    make show
    make all
    echo

    file build/af_xdp_forwarder_kern.bpf.o build/af_xdp_forwarder || true
    echo "BUILD_RESULT=PASS"
} 2>&1 | tee "${out}"