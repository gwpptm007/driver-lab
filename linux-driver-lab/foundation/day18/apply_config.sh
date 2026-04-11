#!/usr/bin/env bash
set -euo pipefail

# Day18 apply_config.sh
# ---------------------
# Day18 的核心不是再追加一轮“经验删项”，而是把第二轮裁剪做成：
# 1. 可分类（必须项 / 平台项 / 调试项 / 性能项）
# 2. 可解释（为什么保留 / 为什么删除）
# 3. 可对照（legacy round2b 与 classified 的关系清晰）
# 4. 完全独立在 day18/ 目录内完成
#
# 当前支持三个 profile：
#   baseline
#       只应用 trace_baseline.fragment，得到可启动、可 tracing、可 perf 的基线。
#
#   round2b_legacy
#       沿用 day17 的 legacy 裁剪表达：baseline + trim_round1 + trim_round2b。
#       这个 profile 主要用于和 day17 结果保持连续性。
#
#   classified
#       使用 Day18 的分类表达：baseline + required + platform + debug + perf + trim_day18。
#       它强调“为什么这样保留/删除”，便于学习、汇报和后续回滚。
#
# 推荐首次使用：
#   cd linux-driver-lab/day18
#   PROFILE=classified BUILD_KERNEL=no ./apply_config.sh
#
# 常用可选项：
#   PROFILE=baseline ./apply_config.sh
#   PROFILE=round2b_legacy ./apply_config.sh
#   PROFILE=classified ./apply_config.sh
#   EXTRA_FRAGMENT=/tmp/my.fragment ./apply_config.sh

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
EXPORT_SAVEDEFCONFIG="${EXPORT_SAVEDEFCONFIG:-yes}"
SAVEDEFCONFIG_DIR="${SAVEDEFCONFIG_DIR:-$SCRIPT_DIR/output/config_snapshots}"

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
        y) printf '%s=y\n' "$key" >> "$tmp" ;;
        m) printf '%s=m\n' "$key" >> "$tmp" ;;
        n) printf '# %s is not set\n' "$key" >> "$tmp" ;;
        *) rm -f "$tmp"; fail "unsupported symbol value: $key=$val" ;;
    esac
    mv "$tmp" "$cfg"
}

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
            *) ;;
        esac
    done < "$fragment"
}

build_fragment_chain() {
    case "$PROFILE" in
        baseline)
            printf '%s\n' "$SCRIPT_DIR/config/trace_baseline.fragment"
            ;;
        round2b_legacy)
            printf '%s\n' \
                "$SCRIPT_DIR/config/trace_baseline.fragment" \
                "$SCRIPT_DIR/config/trim_round1.fragment" \
                "$SCRIPT_DIR/config/trim_round2b.fragment"
            ;;
        classified)
            printf '%s\n' \
                "$SCRIPT_DIR/config/trace_baseline.fragment" \
                "$SCRIPT_DIR/config/10_required.fragment" \
                "$SCRIPT_DIR/config/20_platform.fragment" \
                "$SCRIPT_DIR/config/30_debug.fragment" \
                "$SCRIPT_DIR/config/40_perf.fragment" \
                "$SCRIPT_DIR/config/90_trim_day18.fragment"
            ;;
        *)
            fail "unsupported PROFILE: $PROFILE (expected: baseline/round2b_legacy/classified)"
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

SCRIPTS_CONFIG="$KERNEL_SRC/scripts/config"
CFG="$KERNEL_OUT/.config"

info "KERNEL_DIR   : $KERNEL_DIR"
info "KERNEL_SRC   : $KERNEL_SRC"
info "KERNEL_OUT   : $KERNEL_OUT"
info "KERNEL_IMG   : $KERNEL_IMG"
info "PROFILE      : $PROFILE"
info "BUILD_KERNEL : $BUILD_KERNEL"
info "ARCH         : $ARCH_NAME"
info "CROSS_COMPILE: $CROSS_COMPILE"
info "JOBS         : $JOBS"

while IFS= read -r fragment; do
    [ -n "$fragment" ] || continue
    apply_fragment_file "$CFG" "$fragment"
done < <(build_fragment_chain)

if [ -n "$EXTRA_FRAGMENT" ]; then
    apply_fragment_file "$CFG" "$EXTRA_FRAGMENT"
fi

info "running olddefconfig ..."
make -C "$KERNEL_SRC" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" O="$KERNEL_OUT" olddefconfig

info "effective focus symbols after olddefconfig:"
grep -E '^(CONFIG_(DEBUG_FS|TRACEFS_FS|TRACING|TRACEPOINTS|FTRACE|FUNCTION_TRACER|FUNCTION_GRAPH_TRACER|DYNAMIC_FTRACE|KALLSYMS|KALLSYMS_ALL|PERF_EVENTS|HW_PERF_EVENTS|PCI|NET|SCSI|MODULES|BLK_DEV_INITRD|DEVTMPFS|PROC_FS|SYSFS|TMPFS|OF|SERIAL_AMBA_PL011|ARM_GIC|REGMAP)=|# CONFIG_(PCI|NET|SCSI).+ is not set$)' "$CFG" || true

if [ "$EXPORT_SAVEDEFCONFIG" = "yes" ]; then
    mkdir -p "$SAVEDEFCONFIG_DIR"
    (
        cd "$SAVEDEFCONFIG_DIR"
        rm -f defconfig
        make -C "$KERNEL_SRC" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" O="$KERNEL_OUT" savedefconfig >/dev/null
        if [ -f defconfig ]; then
            mv defconfig "${PROFILE}.savedefconfig"
            info "saved defconfig -> $SAVEDEFCONFIG_DIR/${PROFILE}.savedefconfig"
        fi
    )
fi

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
