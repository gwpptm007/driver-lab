#!/usr/bin/env bash
set -euo pipefail

# Day15 apply_config.sh
# ---------------------
# 这份脚本专门负责“Day15 需要的 tracing/ftrace/function_graph 基线配置”。
#
# 为什么把它单独拆出来？
# 因为 Day15 这一周同时涉及两类事情：
#   1) 内核配置与内核重编
#   2) rootfs / dtb / demo 模块 / QEMU bring-up
#
# 如果把这两类职责揉在同一个脚本里，第一次排错时会很痛苦：
# 你很难判断是“内核没编好”，还是“Day15 实验环境没组好”。
#
# 所以这里故意拆成两段：
#   - ./apply_config.sh : 管内核 config + olddefconfig + build kernel
#   - ./build.sh        : 管 Day15 自己的模块 / rootfs / dtb / QEMU
#
# 当前项目的内核目录不是传统的单层结构，而是：
#   KERNEL_DIR=/.../kernel-src/linux-5.15.10
#   KERNEL_SRC=$KERNEL_DIR/src               # 真正有顶层 Makefile 的源码根
#   KERNEL_OUT=$KERNEL_DIR/build/arm64       # arm64 输出目录（.config / vmlinux / modules ABI）
#   KERNEL_IMG=$KERNEL_DIR/output/arm64/Image
#
# 因此：
#   - olddefconfig / kernel build 必须对 KERNEL_SRC 执行 make -C
#   - 同时显式指定 O=KERNEL_OUT
#
# 推荐用法：
#   cd linux-driver-lab/day15
#   export KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10
#   export CROSS_COMPILE=aarch64-linux-gnu-
#   ./apply_config.sh
#
# 可选：
#   export FRAGMENT=/path/to/another.fragment
#   export JOBS=8

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10}"
KERNEL_SRC="${KERNEL_SRC:-$KERNEL_DIR/src}"
KERNEL_OUT="${KERNEL_OUT:-$KERNEL_DIR/build/arm64}"
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"
FRAGMENT="${FRAGMENT:-$SCRIPT_DIR/config/trace_baseline.fragment}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
ARCH_NAME="${ARCH_NAME:-arm64}"
JOBS="${JOBS:-$(nproc)}"

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

# apply_symbol()
# --------------
# 作用：把 fragment 中的 CONFIG_XXX=y / # CONFIG_XXX is not set
#      应用到当前 .config 中。
#
# 为什么不直接整份覆盖 .config？
# 因为 Day15 只想“补齐最小 tracing 基线”，而不是推翻你已有的 arm64 配置。
# 所以这里采取 fragment 叠加的思路：
#   - 删除旧定义
#   - 写入新的 y/n 结论
#
# 这样可读性和可控性都更好，也方便后面的 D16/D18 继续用 fragment 管理。
apply_symbol() {
    local cfg="$1"
    local key="$2"
    local val="$3"
    local tmp
    tmp=$(mktemp)

    # 先删掉旧定义，避免 .config 中同一个符号重复出现。
    grep -vE "^(# ${key} is not set|${key}=)" "$cfg" > "$tmp" || true
    if [ "$val" = "y" ]; then
        printf '%s=y\n' "$key" >> "$tmp"
    else
        printf '# %s is not set\n' "$key" >> "$tmp"
    fi
    mv "$tmp" "$cfg"
}

[ -d "$KERNEL_DIR" ] || fail "KERNEL_DIR not found: $KERNEL_DIR"
[ -d "$KERNEL_SRC" ] || fail "KERNEL_SRC not found: $KERNEL_SRC"
[ -f "$KERNEL_SRC/Makefile" ] || fail "kernel source Makefile not found: $KERNEL_SRC/Makefile"
[ -d "$KERNEL_OUT" ] || fail "KERNEL_OUT not found: $KERNEL_OUT"
[ -f "$KERNEL_OUT/.config" ] || fail "arm64 .config not found: $KERNEL_OUT/.config"
[ -f "$FRAGMENT" ] || fail "fragment not found: $FRAGMENT"

require_cmd make
require_cmd grep
require_cmd sed
require_cmd awk
require_cmd "${CROSS_COMPILE}gcc"

info "KERNEL_DIR   : $KERNEL_DIR"
info "KERNEL_SRC   : $KERNEL_SRC"
info "KERNEL_OUT   : $KERNEL_OUT"
info "KERNEL_IMG   : $KERNEL_IMG"
info "FRAGMENT     : $FRAGMENT"
info "ARCH         : $ARCH_NAME"
info "CROSS_COMPILE: $CROSS_COMPILE"
info "JOBS         : $JOBS"

CFG="$KERNEL_OUT/.config"

# 逐行解析 fragment。
# 支持三种常见形式：
#   CONFIG_FOO=y
#   CONFIG_BAR=n
#   # CONFIG_BAZ is not set
#
# 说明性注释和空行会被忽略。
while IFS= read -r line; do
    [ -z "$line" ] && continue
    case "$line" in
        '# '*)
            if printf '%s' "$line" | grep -q ' is not set$'; then
                sym=${line#\# }
                sym=${sym% is not set}
                apply_symbol "$CFG" "$sym" "n"
            fi
            continue
            ;;
        CONFIG_*=y)
            sym=${line%%=*}
            apply_symbol "$CFG" "$sym" "y"
            ;;
        CONFIG_*=n)
            sym=${line%%=*}
            apply_symbol "$CFG" "$sym" "n"
            ;;
        *)
            # fragment 里的说明性文字直接忽略。
            continue
            ;;
    esac
done < "$FRAGMENT"

# olddefconfig 的意义：
# - 让 Kconfig 依赖关系重新收敛
# - 把你手工塞进 .config 的符号，转成“真正有效”的最终配置
# - 避免后面 build 时因为依赖不满足而悄悄失效
info "running olddefconfig ..."
make -C "$KERNEL_SRC" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" O="$KERNEL_OUT" olddefconfig

info "effective tracing-related config after olddefconfig:"
grep -E 'CONFIG_(DEBUG_FS|TRACEFS_FS|TRACING|TRACEPOINTS|FTRACE|FUNCTION_TRACER|FUNCTION_GRAPH_TRACER|DYNAMIC_FTRACE|KALLSYMS|KALLSYMS_ALL|PERF_EVENTS|HW_PERF_EVENTS|IRQSOFF_TRACER|SCHED_TRACER)=' "$CFG" || true

# 到这里才正式开始编内核。
# Day15 当前只需要 arm64 Image 能启动 QEMU virt，并支持 tracing/function_graph。
info "building kernel ..."
make -C "$KERNEL_SRC" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" O="$KERNEL_OUT" -j"$JOBS"

# QEMU 启动时统一从 output/arm64/Image 取镜像。
# 如果这次 build 出来了新的 Image，就顺手同步过去，避免 Day15 后续 build.sh 再猜路径。
if [ -f "$KERNEL_OUT/arch/arm64/boot/Image" ]; then
    mkdir -p "$(dirname "$KERNEL_IMG")"
    cp "$KERNEL_OUT/arch/arm64/boot/Image" "$KERNEL_IMG"
    info "synced Image -> $KERNEL_IMG"
fi

info "done"
info "next: cd $SCRIPT_DIR && ./build.sh"
