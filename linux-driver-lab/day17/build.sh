#!/usr/bin/env bash
set -euo pipefail

# Day17 build.sh
# --------------
# Day17 的 build.sh 现在不仅负责 baseline 的 rootfs / 模块 / DTB 组装，
# 还把 perf 的集成正式并进了 Day17 自己的独立工作流。
#
# 这版的 perf 集成目标不是“手工往 rootfs 里塞一个 perf 试试看”，而是：
#
# 1. Day17 自己能发现现成的 arm64 perf；
# 2. 没有现成 perf 时，Day17 可以调用 build_perf.sh 进行最小化构建；
# 3. 把 perf 放进 rootfs 时，不只复制 perf 二进制，还递归复制它的动态依赖；
# 4. 让 guest_collect.sh 能直接验证：
#      perf --version
#      perf list software
#      perf stat -e task-clock -- true
#
# 推荐的“完整 perf 集成”用法：
#
#   cd linux-driver-lab/day17
#   export KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10
#   export BUSYBOX_DIR=~/workspace/driver-lab/kernel-src/busybox-1.36.1
#   export CROSS_COMPILE=aarch64-linux-gnu-
#
#   PROFILE=baseline ./apply_config.sh
#   PERF_REQUIRED=yes PERF_MODE=auto ./build.sh
#
# 上面的 PERF_MODE=auto 会按下面顺序找 perf：
#   1. 你显式传入的 PERF_PATH
#   2. day17/output/perf/perf（之前 build_perf.sh 生成过）
#   3. KERNEL_SRC/tools/perf/perf
#   4. 如果还没找到，则自动调用 build_perf.sh 现编
#
# 如果你只是想跳过 perf，仍可显式：
#   PERF_MODE=skip ./build.sh
#
# 如果你已经自己编好了 perf，可显式：
#   PERF_MODE=external PERF_PATH=/path/to/perf PERF_REQUIRED=yes ./build.sh

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10}"
KERNEL_SRC="${KERNEL_SRC:-$KERNEL_DIR/src}"
BUSYBOX_DIR="${BUSYBOX_DIR:-$REPO_ROOT/../kernel-src/busybox-1.36.1}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
ARCH_NAME="${ARCH_NAME:-arm64}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
DTC_BIN="${DTC_BIN:-dtc}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
READELF_BIN="${READELF_BIN:-readelf}"
ROOTFS="$SCRIPT_DIR/rootfs"
PROFILE="${PROFILE:-baseline}"
QEMU_AUTO_BOOT="${QEMU_AUTO_BOOT:-yes}"

KDIR="${KDIR:-$KERNEL_DIR/build/arm64}"
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"
BUSYBOX_INSTALL="${BUSYBOX_INSTALL:-$BUSYBOX_DIR/output/arm64/_install}"
BUSYBOX_OUTPUT="${BUSYBOX_OUTPUT:-$BUSYBOX_DIR/output/arm64}"
QEMU_MEMORY_MB="${QEMU_MEMORY_MB:-1024}"
QEMU_CPU="${QEMU_CPU:-cortex-a57}"
KERNEL_CMDLINE="${KERNEL_CMDLINE:-console=ttyAMA0 root=/dev/ram0 rw rdinit=/init}"

# perf 相关变量
# --------------
# PERF_MODE 支持三种模式：
#   auto     : 优先找现成 perf，找不到就自动构建
#   external : 只使用 PERF_PATH 指定的 perf
#   skip     : 完全跳过 perf 集成
PERF_MODE="${PERF_MODE:-auto}"
PERF_REQUIRED="${PERF_REQUIRED:-no}"
PERF_PATH="${PERF_PATH:-}"
PERF_LIB_DIRS="${PERF_LIB_DIRS:-}"
PERF_SYSROOT="${PERF_SYSROOT:-}"
PERF_OUTPUT_DIR="${PERF_OUTPUT_DIR:-$SCRIPT_DIR/output/perf}"
PERF_BUILD_SCRIPT="${PERF_BUILD_SCRIPT:-$SCRIPT_DIR/build_perf.sh}"
PERF_MANIFEST="${PERF_MANIFEST:-$SCRIPT_DIR/output/perf/perf_bundle_manifest.txt}"

fail() {
    echo "[ERROR] $*" >&2
    exit 1
}

info() {
    echo "[INFO] $*"
}

warn() {
    echo "[WARN] $*" >&2
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "command not found: $1"
}

# is_aarch64_elf_file()
# --------------------
# 判断一个文件是否为 AArch64 目标文件，避免把宿主机 x86_64 的 libc/loader
# 错打进 arm64 guest rootfs。
is_aarch64_elf_file() {
    local path="$1"
    local info=""

    [ -e "$path" ] || return 1
    info=$(file "$path" 2>/dev/null || true)
    printf '%s' "$info" | grep -Eq 'ARM aarch64|AArch64|arm64'
}

copy_file_keep_parent() {
    local src="$1"
    local dst_root="$2"
    local rel="${src#/}"

    mkdir -p "$dst_root/$(dirname "$rel")"
    cp -L "$src" "$dst_root/$rel"
}

# copy_file_to_target_path()
# --------------------------
# 把宿主机上的一个文件安装到 guest 的“目标机路径”。
# 处理 perf 的 loader/so 时不能按宿主机绝对路径原样复制，
# 否则 guest 中会出现：which perf 能看到，但 perf --version 报 not found。
copy_file_to_target_path() {
    local src="$1"
    local dst_root="$2"
    local target_path="$3"
    local rel="${target_path#/}"

    [ -f "$src" ] || return 1
    [ -n "$target_path" ] || return 1

    mkdir -p "$dst_root/$(dirname "$rel")"
    cp -L "$src" "$dst_root/$rel"
}

# derive_target_path_from_sysroot()
# --------------------------------
# 把 sysroot 下的真实库路径，映射成 guest 里的目标路径。
# 例：
#   PERF_SYSROOT=/usr/aarch64-linux-gnu
#   real_lib=/usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1
#   => target /lib/ld-linux-aarch64.so.1
# 对普通 so 也同理，优先通过 PERF_SYSROOT 剥离前缀得到 target 路径。
# 如果 real_lib 不在 PERF_SYSROOT 下，再从常见 target 库目录中兜底提取。
derive_target_path_from_sysroot() {
    local real_path="$1"
    local tail=""

    if [ -n "$PERF_SYSROOT" ] && [ "$PERF_SYSROOT" != "/" ]; then
        case "$real_path" in
            "$PERF_SYSROOT"/*)
                tail="${real_path#$PERF_SYSROOT}"
                [ -n "$tail" ] && {
                    printf '%s\n' "$tail"
                    return 0
                }
                ;;
        esac
    fi

    case "$real_path" in
        */lib/aarch64-linux-gnu/*)
            printf '%s\n' "/lib/aarch64-linux-gnu/${real_path##*/lib/aarch64-linux-gnu/}"
            return 0
            ;;
        */usr/lib/aarch64-linux-gnu/*)
            printf '%s\n' "/usr/lib/aarch64-linux-gnu/${real_path##*/usr/lib/aarch64-linux-gnu/}"
            return 0
            ;;
        */lib64/*)
            printf '%s\n' "/lib64/${real_path##*/lib64/}"
            return 0
            ;;
        */usr/lib64/*)
            printf '%s\n' "/usr/lib64/${real_path##*/usr/lib64/}"
            return 0
            ;;
        */lib/*)
            printf '%s\n' "/lib/${real_path##*/lib/}"
            return 0
            ;;
        */usr/lib/*)
            printf '%s\n' "/usr/lib/${real_path##*/usr/lib/}"
            return 0
            ;;
    esac

    return 1
}

# find_in_path_list()
# -------------------
# 在冒号分隔的目录列表中递归寻找某个文件名。
# 之所以不用简单的 "$dir/$needle"，是因为常见 sysroot 往往有多架构层级：
#   /usr/lib/aarch64-linux-gnu/
#   /lib/aarch64-linux-gnu/
#   /usr/lib64/
#   /lib64/
# 因此这里允许在目录树内递归寻找一次，避免 PERF_LIB_DIRS 必须写得非常死。
find_in_path_list() {
    local needle="$1"
    local list="$2"
    local old_ifs="$IFS"
    local dir
    local found=""

    IFS=':'
    for dir in $list; do
        [ -n "$dir" ] || continue
        [ -d "$dir" ] || continue
        found=$(find "$dir" -type f -o -type l 2>/dev/null | grep "/${needle}$" | head -n 1 || true)
        if [ -n "$found" ]; then
            printf '%s\n' "$found"
            IFS="$old_ifs"
            return 0
        fi
    done
    IFS="$old_ifs"
    return 1
}

extract_interp() {
    local elf="$1"
    "$READELF_BIN" -l "$elf" 2>/dev/null | awk '/Requesting program interpreter/ { gsub(/\[|\]/, "", $NF); print $NF; exit }'
}

extract_needed_libs() {
    local elf="$1"
    "$READELF_BIN" -d "$elf" 2>/dev/null | awk '/NEEDED/ { gsub(/\[|\]/, "", $NF); print $NF }'
}

# build_default_perf_lib_dirs()
# -----------------------------
# 如果用户没有显式提供 PERF_LIB_DIRS，就尝试根据 sysroot 自动生成一组常见搜索路径。
# 注意：sysroot 为空或退化成 / 时，绝不能盲扫宿主机 /lib /usr/lib，
# 否则很容易把 x86_64 宿主机库误当成 arm64 目标库。
build_default_perf_lib_dirs() {
    local sysroot="$1"
    local dirs=()
    local d

    [ -n "$sysroot" ] || return 0
    [ "$sysroot" != "/" ] || return 0

    for d in         "$sysroot/lib"         "$sysroot/usr/lib"         "$sysroot/lib64"         "$sysroot/usr/lib64"         "$sysroot/lib/aarch64-linux-gnu"         "$sysroot/usr/lib/aarch64-linux-gnu"         "$sysroot/lib/${CROSS_COMPILE%-}"         "$sysroot/usr/lib/${CROSS_COMPILE%-}"
    do
        if [ -d "$d" ]; then
            dirs+=("$d")
        fi
    done

    if [ ${#dirs[@]} -gt 0 ]; then
        local joined
        joined=$(printf '%s:' "${dirs[@]}")
        printf '%s
' "${joined%:}"
    fi
}

# resolve_target_lib_via_cross_gcc()
# ---------------------------------
# 优先让交叉编译器自己解析目标机库路径。
# 例如：aarch64-linux-gnu-gcc -print-file-name=libc.so.6
# 这比盲扫宿主机 /lib /usr/lib 稳得多，也更能避免误混入 x86_64 库。
resolve_target_lib_via_cross_gcc() {
    local libname="$1"
    local resolved=""

    [ -n "$libname" ] || return 1
    resolved=$("${CROSS_COMPILE}gcc" -print-file-name="$libname" 2>/dev/null || true)
    [ -n "$resolved" ] || return 1
    [ "$resolved" != "$libname" ] || return 1
    [ -e "$resolved" ] || return 1
    is_aarch64_elf_file "$resolved" || return 1
    printf '%s
' "$resolved"
}

# resolve_perf_path()
# -------------------
# 按 Day17 的 perf 集成策略，决定最终要带进 rootfs 的 perf 从哪里来。
resolve_perf_path() {
    local candidate=""

    case "$PERF_MODE" in
        skip)
            return 1
            ;;
        external)
            [ -n "$PERF_PATH" ] || fail "PERF_MODE=external 但没有设置 PERF_PATH"
            [ -f "$PERF_PATH" ] || fail "PERF_PATH not found: $PERF_PATH"
            printf '%s\n' "$PERF_PATH"
            return 0
            ;;
        auto)
            if [ -n "$PERF_PATH" ] && [ -f "$PERF_PATH" ]; then
                printf '%s\n' "$PERF_PATH"
                return 0
            fi

            candidate="$PERF_OUTPUT_DIR/perf"
            if [ -f "$candidate" ]; then
                printf '%s\n' "$candidate"
                return 0
            fi

            candidate="$KERNEL_SRC/tools/perf/perf"
            if [ -f "$candidate" ]; then
                printf '%s\n' "$candidate"
                return 0
            fi

            if [ -x "$PERF_BUILD_SCRIPT" ]; then
                info "perf not found yet; invoking $PERF_BUILD_SCRIPT ..."
                "$PERF_BUILD_SCRIPT"
                candidate="$PERF_OUTPUT_DIR/perf"
                if [ -f "$candidate" ]; then
                    printf '%s\n' "$candidate"
                    return 0
                fi
            fi
            return 1
            ;;
        *)
            fail "unsupported PERF_MODE: $PERF_MODE (expected: auto/external/skip)"
            ;;
    esac
}

# install_perf_deps_recursive()
# -----------------------------
# 递归复制一个 ELF 及其动态依赖。
# 为什么要“递归”？因为 perf 的一阶依赖（例如 libelf.so.1）自己还会继续依赖 libc、zlib
# 等库。只复制第一层，很容易导致 guest 中出现：
#   perf: error while loading shared libraries: xxx.so: cannot open shared object file
#
# 这里使用一个 bash 关联数组做 visited 集，避免同一库被重复处理。
declare -A PERF_VISITED=()
install_perf_deps_recursive() {
    local elf="$1"
    local interp
    local lib
    local real_lib
    local target_lib_path=""

    [ -f "$elf" ] || return 0

    if [ -n "${PERF_VISITED[$elf]:-}" ]; then
        return 0
    fi
    PERF_VISITED[$elf]=1

    interp=$(extract_interp "$elf" || true)
    if [ -n "$interp" ]; then
        real_lib=$(resolve_target_lib_via_cross_gcc "$(basename "$interp")" || true)
        if [ -z "$real_lib" ]; then
            real_lib=$(find_in_path_list "$(basename "$interp")" "$PERF_LIB_DIRS" || true)
        fi
        if [ -n "$real_lib" ]; then
            if ! is_aarch64_elf_file "$real_lib"; then
                warn "skip non-aarch64 interpreter candidate: $real_lib"
                printf 'reject-interp-non-aarch64 %s -> %s\n' "$interp" "$real_lib" >> "$PERF_MANIFEST"
            else
                copy_file_to_target_path "$real_lib" "$ROOTFS" "$interp"
                printf 'interp %s -> %s (target %s)\n' "$interp" "$real_lib" "$interp" >> "$PERF_MANIFEST"
                install_perf_deps_recursive "$real_lib"
            fi
        else
            warn "perf interpreter not found for target: $interp"
            printf 'missing-interp %s\n' "$interp" >> "$PERF_MANIFEST"
        fi
    fi

    while IFS= read -r lib; do
        [ -n "$lib" ] || continue
        real_lib=$(resolve_target_lib_via_cross_gcc "$lib" || true)
        if [ -z "$real_lib" ]; then
            real_lib=$(find_in_path_list "$lib" "$PERF_LIB_DIRS" || true)
        fi
        if [ -n "$real_lib" ]; then
            if ! is_aarch64_elf_file "$real_lib"; then
                warn "skip non-aarch64 dependency candidate: $real_lib"
                printf 'reject-needed-non-aarch64 %s -> %s\n' "$lib" "$real_lib" >> "$PERF_MANIFEST"
                continue
            fi

            target_lib_path=$(derive_target_path_from_sysroot "$real_lib" || true)
            if [ -z "$target_lib_path" ]; then
                case "$lib" in
                    ld-linux-aarch64.so.1)
                        target_lib_path="/lib/ld-linux-aarch64.so.1"
                        ;;
                    *)
                        target_lib_path="/lib/$lib"
                        ;;
                esac
            fi
            copy_file_to_target_path "$real_lib" "$ROOTFS" "$target_lib_path"
            printf 'needed %s -> %s (target %s)\n' "$lib" "$real_lib" "$target_lib_path" >> "$PERF_MANIFEST"
            install_perf_deps_recursive "$real_lib"
        else
            warn "perf dependency not found for target: $lib"
            printf 'missing-needed %s\n' "$lib" >> "$PERF_MANIFEST"
        fi
    done < <(extract_needed_libs "$elf")
}

install_perf_into_rootfs() {
    local perf="$1"
    local perf_file_info

    [ -f "$perf" ] || fail "resolved perf path not found: $perf"
    require_cmd "$READELF_BIN"
    require_cmd file

    perf_file_info=$(file "$perf" || true)
    info "resolved perf       : $perf"
    echo "$perf_file_info"

    if ! printf '%s' "$perf_file_info" | grep -Eq 'ARM aarch64|ARM64|aarch64'; then
        fail "resolved perf is not an arm64/aarch64 binary: $perf"
    fi

    mkdir -p "$ROOTFS/usr/bin" "$(dirname "$PERF_MANIFEST")"
    : > "$PERF_MANIFEST"
    cp -L "$perf" "$ROOTFS/usr/bin/perf"
    chmod +x "$ROOTFS/usr/bin/perf"
    printf 'perf %s\n' "$perf" >> "$PERF_MANIFEST"

    if [ -z "$PERF_LIB_DIRS" ]; then
        if [ -z "$PERF_SYSROOT" ]; then
            PERF_SYSROOT="$(${CROSS_COMPILE}gcc -print-sysroot 2>/dev/null || true)"
        fi
        PERF_LIB_DIRS="$(build_default_perf_lib_dirs "$PERF_SYSROOT")"
    fi

    [ -n "$PERF_LIB_DIRS" ] || fail "perf selected but PERF_LIB_DIRS could not be determined; set PERF_LIB_DIRS or PERF_SYSROOT explicitly"

    info "PERF_SYSROOT        : ${PERF_SYSROOT:-auto-empty}"
    info "PERF_LIB_DIRS       : $PERF_LIB_DIRS"
    printf 'perf_lib_dirs %s\n' "$PERF_LIB_DIRS" >> "$PERF_MANIFEST"

    install_perf_deps_recursive "$perf"

    # 把 manifest 顺手放进 guest，后续如果 perf 在 guest 中执行失败，可以直接看清：
    # - perf 本体来自哪
    # - 解释器/依赖都复制了哪些
    # - 哪些库当时没找到
    mkdir -p "$ROOTFS/etc"
    cp -L "$PERF_MANIFEST" "$ROOTFS/etc/day17_perf_manifest.txt"
}

[ -d "$KERNEL_DIR" ] || fail "KERNEL_DIR not found: $KERNEL_DIR"
[ -d "$KERNEL_SRC" ] || fail "KERNEL_SRC not found: $KERNEL_SRC"
[ -d "$BUSYBOX_DIR" ] || fail "BUSYBOX_DIR not found: $BUSYBOX_DIR"
[ -d "$KDIR" ] || fail "arm64 kernel build dir not found: $KDIR"

require_cmd "$QEMU_BIN"
require_cmd "$DTC_BIN"
require_cmd "$PYTHON_BIN"
require_cmd "${CROSS_COMPILE}gcc"
require_cmd file
require_cmd cpio

if [ ! -f "$KERNEL_IMG" ] && [ -f "$KDIR/arch/arm64/boot/Image" ]; then
    mkdir -p "$(dirname "$KERNEL_IMG")"
    cp "$KDIR/arch/arm64/boot/Image" "$KERNEL_IMG"
fi
[ -f "$KERNEL_IMG" ] || fail "arm64 kernel Image not found: $KERNEL_IMG"

BUSYBOX_CANDIDATES=(
    "$BUSYBOX_INSTALL/bin/busybox"
    "$BUSYBOX_OUTPUT/busybox"
    "$BUSYBOX_DIR/build/arm64/busybox"
)
BUSYBOX_PATH=""
for p in "${BUSYBOX_CANDIDATES[@]}"; do
    if [ -x "$p" ]; then
        BUSYBOX_PATH="$p"
        break
    fi
done
[ -n "$BUSYBOX_PATH" ] || fail "arm64 busybox not found under: $BUSYBOX_DIR"

BUSYBOX_FILE_INFO=$(file "$BUSYBOX_PATH" || true)
info "PROFILE             : $PROFILE"
info "KERNEL_DIR          : $KERNEL_DIR"
info "KERNEL_SRC          : $KERNEL_SRC"
info "BUSYBOX_DIR         : $BUSYBOX_DIR"
info "Kernel build dir    : $KDIR"
info "Kernel image        : $KERNEL_IMG"
info "BusyBox             : $BUSYBOX_PATH"
info "CROSS_COMPILE       : $CROSS_COMPILE"
info "QEMU                : $QEMU_BIN"
info "QEMU memory         : ${QEMU_MEMORY_MB}MB"
info "QEMU cpu            : $QEMU_CPU"
info "PERF_MODE           : $PERF_MODE"
info "PERF_REQUIRED       : $PERF_REQUIRED"
echo "$BUSYBOX_FILE_INFO"
if echo "$BUSYBOX_FILE_INFO" | grep -q "dynamically linked"; then
    fail "当前 busybox 是动态链接版，不能直接用于最小 initramfs/rootfs"
fi

# 1) 编译教学模块
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" clean
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE"

# 2) 重新构造 rootfs
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,sbin,etc,tmp,usr/bin,usr/lib,lib,lib64}
mkdir -p "$ROOTFS/sys/kernel/debug"

cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"

# BusyBox applet 链接
# ------------------
# 这里显式补上 true/false，原因是 Day17 已经把 perf smoke 测试并入自动化链路。
# perf stat 后面需要跟一个最小 workload；之前 rootfs 里虽然 busybox --list 能看到 true，
# 但没有 /bin/true 这个 applet 链接，于是 guest_collect.sh 中的 perf smoke 会报：
#   Workload failed: No such file or directory
# 现在把 /bin/true 固化进 rootfs 后，guest_collect.sh 就能直接跑：
#   perf stat -e task-clock -- /bin/true
# 不再需要你进 guest 手工 ln -sf /bin/busybox /bin/true。
for cmd in \
    sh ls cat echo mount insmod rmmod dmesg chmod sleep ps mkdir grep date rm touch sync \
    uname poweroff reboot tail head usleep cp wc sed cut find env printf pwd ln clear \
    which awk xargs tr sort uniq true false; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

# 3) Day17 自身文件进入 rootfs
cp "$SCRIPT_DIR/demo_regmap.ko" "$ROOTFS/"
cp "$SCRIPT_DIR/collect/guest_collect.sh" "$ROOTFS/bin/day17_guest_collect.sh"
cp "$SCRIPT_DIR/function_graph_targets.txt" "$ROOTFS/etc/day17_function_graph_targets.txt"
chmod +x "$ROOTFS/bin/day17_guest_collect.sh"

# 4) 正式集成 perf
RESOLVED_PERF=""
if RESOLVED_PERF=$(resolve_perf_path 2>/dev/null); then
    info "installing perf into rootfs ..."
    install_perf_into_rootfs "$RESOLVED_PERF"
else
    if [ "$PERF_MODE" != "skip" ] && [ "$PERF_REQUIRED" = "yes" ]; then
        fail "perf integration requested but no usable perf binary was found/built; try running ./build_perf.sh first or set PERF_PATH"
    fi
    if [ "$PERF_MODE" != "skip" ]; then
        warn "perf not integrated this time; guest will likely show perf_bin_ok=no"
    else
        info "PERF_MODE=skip, perf integration skipped by request"
    fi
fi

cat > "$ROOTFS/init" <<'EOT'
#!/bin/sh
# Day17 guest /init
# -----------------
# 这份 /init 除了做最小系统挂载外，还顺手提示 perf 的验证入口，方便你进 guest 后
# 第一时间确认 perf 是否真正被带进来了。
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t debugfs debugfs /sys/kernel/debug 2>/dev/null || true
mount -t tracefs tracefs /sys/kernel/tracing 2>/dev/null || true

echo "==============================================================="
echo " Linux Driver Lab Day17 guest ready (self-contained)"
echo "==============================================================="
echo "1) insmod /demo_regmap.ko"
echo "2) /bin/day17_guest_collect.sh"
echo "3) cat /tmp/day17-baseline/metrics.env"
echo "4) which perf && perf --version && perf stat -e task-clock -- true"
echo
exec /bin/sh
EOT
chmod +x "$ROOTFS/init"

# 5) 打包 rootfs
(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)

# 6) 导出/注入 Day17 DT
rm -f "$SCRIPT_DIR/virt-base.dtb" "$SCRIPT_DIR/virt-base.dts" "$SCRIPT_DIR/virt-day17.dts" "$SCRIPT_DIR/virt-day17.dtb"
"$QEMU_BIN" -machine virt,dumpdtb="$SCRIPT_DIR/virt-base.dtb" -cpu "$QEMU_CPU" -m "$QEMU_MEMORY_MB" -nographic >/dev/null 2>&1 || true
[ -f "$SCRIPT_DIR/virt-base.dtb" ] || fail "failed to dump QEMU virt base DTB"

"$DTC_BIN" -I dtb -O dts -o "$SCRIPT_DIR/virt-base.dts" "$SCRIPT_DIR/virt-base.dtb"
"$PYTHON_BIN" "$SCRIPT_DIR/inject_virt_dt.py" \
    --input "$SCRIPT_DIR/virt-base.dts" \
    --fragment "$SCRIPT_DIR/demo_regmap.fragment.dtsi" \
    --output "$SCRIPT_DIR/virt-day17.dts"
"$DTC_BIN" -I dts -O dtb -o "$SCRIPT_DIR/virt-day17.dtb" "$SCRIPT_DIR/virt-day17.dts"

info "rootfs image        : $SCRIPT_DIR/rootfs.img"
info "dtb image           : $SCRIPT_DIR/virt-day17.dtb"
if [ -n "$RESOLVED_PERF" ]; then
    info "perf integrated     : yes ($RESOLVED_PERF)"
    info "perf manifest       : $PERF_MANIFEST"
else
    info "perf integrated     : no"
fi

if [ "$QEMU_AUTO_BOOT" = "yes" ]; then
    info "QEMU_AUTO_BOOT=yes, launching guest ..."
    exec "$SCRIPT_DIR/run_qemu.sh"
else
    info "QEMU_AUTO_BOOT=no, build finished without starting QEMU"
fi
