#!/usr/bin/env bash
set -euo pipefail

# Day17 build_perf.sh
# -------------------
# 这份脚本把“如何在本地为 Day17 构建一个 arm64 perf”单独收口出来。
# 这样 build.sh 不需要把所有 perf 构建细节都塞在自己体内，你也可以独立做下面两件事：
#
# 1. 先单独把 perf 编好，确认 perf 本体没问题；
# 2. 再回到 build.sh，把 perf 打进 rootfs。
#
# 设计目标：
# - 优先构建出“够 Day17 用”的最小 perf；
# - 尽量关闭 Python / Perl / GTK / slang / libbpf 等对当前实验无关的功能；
# - 产物稳定落在 day17/output/perf/perf；
# - 构建完后把 file/readelf 结果也落盘，方便排错。
#
# 推荐直接执行：
#   ./build_perf.sh
#
# 或者显式指定环境：
#   KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10 \
#   CROSS_COMPILE=aarch64-linux-gnu- \
#   ./build_perf.sh

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10}"
KERNEL_SRC="${KERNEL_SRC:-$KERNEL_DIR/src}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
ARCH_NAME="${ARCH_NAME:-arm64}"
PERF_SRC_DIR="${PERF_SRC_DIR:-$KERNEL_SRC/tools/perf}"
PERF_OUTPUT_DIR="${PERF_OUTPUT_DIR:-$SCRIPT_DIR/output/perf}"
READELF_BIN="${READELF_BIN:-readelf}"
JOBS="${JOBS:-$(nproc)}"

# 这些 NO_* 选项不是为了“追求 perf 功能最少”，而是为了减少 Day17 当前不需要的依赖面。
# 目标是优先跑通：
#   perf --version
#   perf list software
#   perf stat -e task-clock -- true
PERF_MAKE_FLAGS=(
    "ARCH=$ARCH_NAME"
    "CROSS_COMPILE=$CROSS_COMPILE"
    "OUTPUT=$PERF_OUTPUT_DIR/"
    "prefix=/usr"
    "WERROR=0"
    "NO_LIBPERL=1"
    "NO_LIBPYTHON=1"
    "NO_GTK2=1"
    "NO_SLANG=1"
    "NO_LIBBPF=1"
    "NO_LIBNUMA=1"
    "NO_LIBAUDIT=1"
    "NO_LIBUNWIND=1"
    "NO_LIBDW_DWARF_UNWIND=1"
    "NO_DEMANGLE=1"
)

fail() {
    echo "[ERROR] $*" >&2
    exit 1
}

info() {
    echo "[INFO] $*"
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "command not found: $1"
}

[ -d "$KERNEL_DIR" ] || fail "KERNEL_DIR not found: $KERNEL_DIR"
[ -d "$KERNEL_SRC" ] || fail "KERNEL_SRC not found: $KERNEL_SRC"
[ -d "$PERF_SRC_DIR" ] || fail "PERF_SRC_DIR not found: $PERF_SRC_DIR"
[ -f "$PERF_SRC_DIR/Makefile" ] || fail "perf Makefile not found: $PERF_SRC_DIR/Makefile"

require_cmd make
require_cmd "${CROSS_COMPILE}gcc"
require_cmd file
require_cmd "$READELF_BIN"

mkdir -p "$PERF_OUTPUT_DIR"

info "KERNEL_DIR          : $KERNEL_DIR"
info "KERNEL_SRC          : $KERNEL_SRC"
info "PERF_SRC_DIR        : $PERF_SRC_DIR"
info "PERF_OUTPUT_DIR     : $PERF_OUTPUT_DIR"
info "CROSS_COMPILE       : $CROSS_COMPILE"
info "ARCH_NAME           : $ARCH_NAME"
info "JOBS                : $JOBS"

# 先做一遍 clean，避免历史产物混进来。
make -C "$PERF_SRC_DIR" "${PERF_MAKE_FLAGS[@]}" clean >/dev/null 2>&1 || true
make -C "$PERF_SRC_DIR" -j"$JOBS" "${PERF_MAKE_FLAGS[@]}" perf

[ -f "$PERF_OUTPUT_DIR/perf" ] || fail "perf build finished but binary not found: $PERF_OUTPUT_DIR/perf"

file "$PERF_OUTPUT_DIR/perf" | tee "$PERF_OUTPUT_DIR/perf.file.txt"
"$READELF_BIN" -l "$PERF_OUTPUT_DIR/perf" | tee "$PERF_OUTPUT_DIR/perf.program_headers.txt" >/dev/null
"$READELF_BIN" -d "$PERF_OUTPUT_DIR/perf" | tee "$PERF_OUTPUT_DIR/perf.dynamic.txt" >/dev/null

info "built perf          : $PERF_OUTPUT_DIR/perf"
info "file report         : $PERF_OUTPUT_DIR/perf.file.txt"
info "dynamic report      : $PERF_OUTPUT_DIR/perf.dynamic.txt"
