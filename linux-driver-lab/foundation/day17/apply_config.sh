#!/usr/bin/env bash
set -euo pipefail

# Day17 apply_config.sh
# ---------------------
# 这份脚本负责把 Day17 需要的内核配置统一收口到一个入口里。它和 day15 的区别是：
#
# 1. day15 只关心 baseline tracing 配置；
# 2. day17 需要把 baseline + round1 + round2b 三种 profile 都放进自己的目录；
# 3. day17 还要求文档、脚本、结果目录全部自洽，不再默认依赖 day15/day16。
#
# 所以这份脚本做了三件事：
#
#   PROFILE=baseline ./apply_config.sh
#       只应用 trace_baseline.fragment，得到“可启动、可 tracing”的基线配置。
#
#   PROFILE=round1 ./apply_config.sh
#       先应用 trace_baseline.fragment，再叠加 trim_round1.fragment，
#       得到 Day17 第一轮裁剪配置。
#
#   PROFILE=round2b ./apply_config.sh
#       先应用 baseline，再叠加 round1，再叠加 trim_round2b.fragment，
#       得到 Day17 第二轮增强裁剪配置。
#
# 你可以把它理解成：
# “Day17 的 profile 选择器 + fragment 叠加器 + olddefconfig 收敛器 + 内核构建入口”。
#
# 推荐第一次使用：
#   cd linux-driver-lab/day17
#   export KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10
#   export CROSS_COMPILE=aarch64-linux-gnu-
#   PROFILE=baseline ./apply_config.sh
#
# 常用可选项：
#   PROFILE=round1 ./apply_config.sh
#   PROFILE=round2b ./apply_config.sh
#   BUILD_KERNEL=no PROFILE=round2b ./apply_config.sh      # 只改 .config，不立刻编译
#   EXTRA_FRAGMENT=/tmp/my-test.fragment ./apply_config.sh # 在 profile 末尾额外叠一层

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10}"
KERNEL_SRC="${KERNEL_SRC:-$KERNEL_DIR/src}"
KERNEL_OUT="${KERNEL_OUT:-$KERNEL_DIR/build/arm64}"
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
ARCH_NAME="${ARCH_NAME:-arm64}"
PROFILE="${PROFILE:-baseline}"
BUILD_KERNEL="${BUILD_KERNEL:-yes}"
EXTRA_FRAGMENT="${EXTRA_FRAGMENT:-}"
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
# 把 fragment 里的一条 CONFIG_XXX 结论应用到 .config。
#
# 为什么不用整份覆盖？
# 因为 Day17 的目标是“在既有 arm64 baseline 上做 profile 叠加”，而不是推翻你现有
# 的内核配置。fragment 方式更适合做：
# - baseline 与 trim 的分层；
# - round1 / round2b 的叠加；
# - 后续 day18/day19 继续扩展时的可维护性。
apply_symbol() {
    local cfg="$1"
    local key="$2"
    local val="$3"
    local tmp

    if [ -x "$SCRIPTS_CONFIG" ]; then
        case "$val" in
            y) "$SCRIPTS_CONFIG" --file "$cfg" -e "${key#CONFIG_}" ;;
            m) "$SCRIPTS_CONFIG" --file "$cfg" -m "${key#CONFIG_}" ;;
            n) "$SCRIPTS_CONFIG" --file "$cfg" -d "${key#CONFIG_}" ;;
            *) fail "unsupported symbol value: $key=$val" ;;
        esac
        return
    fi

    tmp=$(mktemp)
    grep -vE "^(# ${key} is not set|${key}=)" "$cfg" > "$tmp" || true
    case "$val" in
        y)
            printf '%s=y
' "$key" >> "$tmp"
            ;;
        m)
            printf '%s=m
' "$key" >> "$tmp"
            ;;
        n)
            printf '# %s is not set
' "$key" >> "$tmp"
            ;;
        *)
            rm -f "$tmp"
            fail "unsupported symbol value: $key=$val"
            ;;
    esac
    mv "$tmp" "$cfg"
}

# apply_fragment_file()
# ---------------------
# 逐行读取一个 fragment，并把其中的 y/n 结论应用到 .config。
# 这里只支持最常见、也最适合教学复盘的三种格式：
#   CONFIG_FOO=y
#   CONFIG_BAR=m
#   CONFIG_BAZ=n
#   # CONFIG_QUX is not set
apply_fragment_file() {
    local cfg="$1"
    local fragment="$2"
    local line sym

    [ -f "$fragment" ] || fail "fragment not found: $fragment"
    info "applying fragment : $fragment"

    while IFS= read -r line; do
        [ -z "$line" ] && continue
        case "$line" in
            '# '*)
                if printf '%s' "$line" | grep -q ' is not set$'; then
                    sym=${line#\# }
                    sym=${sym% is not set}
                    apply_symbol "$cfg" "$sym" "n"
                fi
                ;;
            CONFIG_*=y)
                sym=${line%%=*}
                apply_symbol "$cfg" "$sym" "y"
                ;;
            CONFIG_*=m)
                sym=${line%%=*}
                apply_symbol "$cfg" "$sym" "m"
                ;;
            CONFIG_*=n)
                sym=${line%%=*}
                apply_symbol "$cfg" "$sym" "n"
                ;;
            *)
                # 说明性文字、分组标题、空行都直接跳过。
                ;;
        esac
    done < "$fragment"
}

build_fragment_chain() {
    case "$PROFILE" in
        baseline)
            printf '%s
' "$SCRIPT_DIR/config/trace_baseline.fragment"
            ;;
        round1)
            printf '%s
'                 "$SCRIPT_DIR/config/trace_baseline.fragment"                 "$SCRIPT_DIR/config/trim_round1.fragment"
            ;;
        round2b)
            printf '%s
'                 "$SCRIPT_DIR/config/trace_baseline.fragment"                 "$SCRIPT_DIR/config/trim_round1.fragment"                 "$SCRIPT_DIR/config/trim_round2b.fragment"
            ;;
        *)
            fail "unsupported PROFILE: $PROFILE (expected: baseline/round1/round2b)"
            ;;
    esac
}

[ -d "$KERNEL_DIR" ] || fail "KERNEL_DIR not found: $KERNEL_DIR"
[ -d "$KERNEL_SRC" ] || fail "KERNEL_SRC not found: $KERNEL_SRC"
[ -f "$KERNEL_SRC/Makefile" ] || fail "kernel source Makefile not found: $KERNEL_SRC/Makefile"
[ -d "$KERNEL_OUT" ] || fail "KERNEL_OUT not found: $KERNEL_OUT"
[ -f "$KERNEL_OUT/.config" ] || fail "arm64 .config not found: $KERNEL_OUT/.config"

require_cmd make
require_cmd grep
require_cmd sed
require_cmd awk
require_cmd "${CROSS_COMPILE}gcc"

SCRIPTS_CONFIG="${KERNEL_SRC}/scripts/config"

info "KERNEL_DIR   : $KERNEL_DIR"
info "KERNEL_SRC   : $KERNEL_SRC"
info "KERNEL_OUT   : $KERNEL_OUT"
info "KERNEL_IMG   : $KERNEL_IMG"
info "PROFILE      : $PROFILE"
info "BUILD_KERNEL : $BUILD_KERNEL"
info "ARCH         : $ARCH_NAME"
info "CROSS_COMPILE: $CROSS_COMPILE"
info "JOBS         : $JOBS"

CFG="$KERNEL_OUT/.config"
while IFS= read -r fragment; do
    [ -n "$fragment" ] || continue
    apply_fragment_file "$CFG" "$fragment"
done < <(build_fragment_chain)

if [ -n "$EXTRA_FRAGMENT" ]; then
    apply_fragment_file "$CFG" "$EXTRA_FRAGMENT"
fi

info "running olddefconfig ..."
make -C "$KERNEL_SRC" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" O="$KERNEL_OUT" olddefconfig

info "effective tracing/perf-related config after olddefconfig:"
grep -E 'CONFIG_(DEBUG_FS|TRACEFS_FS|TRACING|TRACEPOINTS|FTRACE|FUNCTION_TRACER|FUNCTION_GRAPH_TRACER|DYNAMIC_FTRACE|KALLSYMS|KALLSYMS_ALL|PERF_EVENTS|HW_PERF_EVENTS|IRQSOFF_TRACER|SCHED_TRACER)=' "$CFG" || true

info "focus symbols for current PROFILE after olddefconfig:"
grep -E '^(CONFIG_(USB_|HID|INPUT_|FB|PCI_|SCSI_|NVME|BTRFS_FS|I2C_)|# CONFIG_(USB_|HID|INPUT_|FB|PCI_|SCSI_|NVME|BTRFS_FS|I2C_).+ is not set)' "$CFG" | sed -n '1,200p' || true

if [ "$BUILD_KERNEL" = "yes" ]; then
    info "building kernel ..."
    make -C "$KERNEL_SRC" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" O="$KERNEL_OUT" -j"$JOBS"

    if [ -f "$KERNEL_OUT/arch/arm64/boot/Image" ]; then
        mkdir -p "$(dirname "$KERNEL_IMG")"
        cp "$KERNEL_OUT/arch/arm64/boot/Image" "$KERNEL_IMG"
        info "synced Image -> $KERNEL_IMG"
    fi
else
    info "BUILD_KERNEL=no, skip kernel compilation"
fi

info "done"
info "next: cd $SCRIPT_DIR && PROFILE=$PROFILE ./build.sh"
