#!/bin/bash
set -euo pipefail

# Day15 build.sh
# --------------
# 这一版 Day15 明确按“自包含实验目录”来组织：
# - 不再默认依赖 day13/build.sh
# - 不再要求 day13/rootfs.img 或 day13/virt-day13.dtb 已经存在
# - 只要你进入 day15/ 目录，就可以独立完成：
#   1) 编译 demo_regmap.ko
#   2) 构造最小 BusyBox rootfs
#   3) 把 Day15 的 guest 采样脚本打进 rootfs
#   4) 生成 virt-day15.dtb
#   5) 启动 QEMU
#
# 这样做的目的：
# - Day15 作为 W3 baseline 实验，应当有自己独立的输入和输出
# - 避免“day15 的问题，其实是 day13 的脚本或产物带来的”混淆
# - 后面 D16/D17/D18 继续迭代时，也更容易只在 day15/ 内做修改
#
# 推荐用法：
#   export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
#   export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
#   export CROSS_COMPILE=aarch64-linux-gnu-
#   ./build.sh
#
# 常见输出：
#   day15/rootfs.img
#   day15/virt-day15.dtb
#   day15/rootfs/
#   QEMU 直接启动进入 guest shell

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../../kernel-src/linux-5.15.10}"
BUSYBOX_DIR="${BUSYBOX_DIR:-$REPO_ROOT/../../kernel-src/busybox-1.36.1}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
ARCH_NAME="${ARCH_NAME:-arm64}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
DTC_BIN="${DTC_BIN:-dtc}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
ROOTFS="$SCRIPT_DIR/rootfs"

KDIR="${KDIR:-$KERNEL_DIR/build/arm64}"
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"
BUSYBOX_INSTALL="${BUSYBOX_INSTALL:-$BUSYBOX_DIR/output/arm64/_install}"
BUSYBOX_OUTPUT="${BUSYBOX_OUTPUT:-$BUSYBOX_DIR/output/arm64}"
QEMU_MEMORY_MB="${QEMU_MEMORY_MB:-1024}"
QEMU_CPU="${QEMU_CPU:-cortex-a57}"
KERNEL_CMDLINE="${KERNEL_CMDLINE:-console=ttyAMA0 root=/dev/ram0 rw rdinit=/init}"

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
[ -d "$BUSYBOX_DIR" ] || fail "BUSYBOX_DIR not found: $BUSYBOX_DIR"
[ -d "$KDIR" ] || fail "arm64 kernel build dir not found: $KDIR"

require_cmd "$QEMU_BIN"
require_cmd "$DTC_BIN"
require_cmd "$PYTHON_BIN"
require_cmd "${CROSS_COMPILE}gcc"
require_cmd file

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
info "KERNEL_DIR          : $KERNEL_DIR"
info "BUSYBOX_DIR         : $BUSYBOX_DIR"
info "Kernel build dir    : $KDIR"
info "Kernel image        : $KERNEL_IMG"
info "BusyBox             : $BUSYBOX_PATH"
info "CROSS_COMPILE       : $CROSS_COMPILE"
info "QEMU                : $QEMU_BIN"
info "QEMU memory         : ${QEMU_MEMORY_MB}MB"
info "QEMU cpu            : $QEMU_CPU"
echo "$BUSYBOX_FILE_INFO"
if echo "$BUSYBOX_FILE_INFO" | grep -q "dynamically linked"; then
    fail "当前 busybox 是动态链接版，不能直接用于最小 initramfs/rootfs"
fi

# 1) 编译 Day15 自己的模块，不再进入 day13/ 编译。
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" clean
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE"

# 2) 重新构造 Day15 自己的 rootfs。
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,sbin,etc,tmp}
mkdir -p "$ROOTFS/sys/kernel/debug"

cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"

# BusyBox applet 链接尽量保守一点：
# 既满足 Day15 当前实验，也避免第一次就引入太多命令依赖。
for cmd in sh ls cat echo mount insmod rmmod dmesg chmod sleep ps mkdir grep date rm touch sync uname poweroff reboot tail head usleep cp wc sed cut find ; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

# 把 Day15 需要的文件都从 day15/ 自己目录内拷贝进去。
cp demo_regmap.ko "$ROOTFS/"
cp collect/guest_collect.sh "$ROOTFS/bin/day15_guest_collect.sh"
cp function_graph_targets.txt "$ROOTFS/etc/day15_function_graph_targets.txt"
chmod +x "$ROOTFS/bin/day15_guest_collect.sh"

cat > "$ROOTFS/init" <<'EOT'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t debugfs debugfs /sys/kernel/debug 2>/dev/null || true
mount -t tracefs tracefs /sys/kernel/tracing 2>/dev/null || true

echo "==============================================================="
echo " Linux Driver Lab Day15 baseline ready (self-contained)"
echo "==============================================================="
echo "1) insmod /demo_regmap.ko"
echo "2) cat /sys/kernel/debug/demo_regmap/snapshot"
echo "3) echo 1 > /sys/kernel/debug/demo_regmap/trigger"
echo "4) /bin/day15_guest_collect.sh"
echo "5) cat /tmp/day15-baseline/metrics.env"
echo "==============================================================="
exec /bin/sh
EOT
chmod +x "$ROOTFS/init"

(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)

# 3) 生成 Day15 自己的 DTB。
rm -f virt-base.dtb virt-base.dts virt-day15.dts virt-day15.dtb
"$QEMU_BIN" -machine virt,dumpdtb=virt-base.dtb -cpu "$QEMU_CPU" -m 512 -nographic >/dev/null 2>&1 || true
[ -f virt-base.dtb ] || fail "failed to dump QEMU virt base DTB"

"$DTC_BIN" -I dtb -O dts -o virt-base.dts virt-base.dtb
"$PYTHON_BIN" inject_virt_dt.py \
    --input virt-base.dts \
    --fragment demo_regmap.fragment.dtsi \
    --output virt-day15.dts
"$DTC_BIN" -I dts -O dtb -o virt-day15.dtb virt-day15.dts

# 4) 直接启动 QEMU，方便第一次手工进入 guest 验证。
"$QEMU_BIN" \
    -machine virt \
    -cpu "$QEMU_CPU" \
    -m "$QEMU_MEMORY_MB" \
    -nographic \
    -kernel "$KERNEL_IMG" \
    -dtb virt-day15.dtb \
    -initrd rootfs.img \
    -append "$KERNEL_CMDLINE"
