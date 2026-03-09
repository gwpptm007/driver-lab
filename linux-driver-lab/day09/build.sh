#!/bin/bash
set -e

# Day09: 第一次在 x86 Ubuntu 宿主机上跑 ARM64 QEMU 的最小 DT 实验
#
# 建议先导出下面三个变量：
#   export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
#   export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
#   export CROSS_COMPILE=aarch64-linux-gnu-
#   ./build.sh
#
# 目录约定：
#   $KERNEL_DIR/build/arm64            arm64 内核构建目录
#   $KERNEL_DIR/output/arm64/Image     arm64 内核镜像
#   $BUSYBOX_DIR/output/arm64/_install arm64 BusyBox 安装目录

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10}"
BUSYBOX_DIR="${BUSYBOX_DIR:-$REPO_ROOT/../kernel-src/busybox-1.36.1}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
ARCH_NAME="${ARCH_NAME:-arm64}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
DTC_BIN="${DTC_BIN:-dtc}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
ROOTFS="./rootfs"

KDIR="${KDIR:-$KERNEL_DIR/build/arm64}"
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"
BUSYBOX_INSTALL="${BUSYBOX_INSTALL:-$BUSYBOX_DIR/output/arm64/_install}"
BUSYBOX_OUTPUT="${BUSYBOX_OUTPUT:-$BUSYBOX_DIR/output/arm64}"

if [ ! -d "$KERNEL_DIR" ]; then
    echo "[ERROR] KERNEL_DIR not found: $KERNEL_DIR"
    exit 1
fi

if [ ! -d "$BUSYBOX_DIR" ]; then
    echo "[ERROR] BUSYBOX_DIR not found: $BUSYBOX_DIR"
    exit 1
fi

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
    echo "[ERROR] $QEMU_BIN not found"
    echo "[HINT ] sudo apt install qemu-system-arm"
    exit 1
fi

if ! command -v "$DTC_BIN" >/dev/null 2>&1; then
    echo "[ERROR] $DTC_BIN not found"
    echo "[HINT ] sudo apt install device-tree-compiler"
    exit 1
fi

if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
    echo "[ERROR] $PYTHON_BIN not found"
    exit 1
fi

if ! command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1; then
    echo "[ERROR] ${CROSS_COMPILE}gcc not found"
    echo "[HINT ] sudo apt install gcc-aarch64-linux-gnu"
    exit 1
fi

if [ ! -d "$KDIR" ]; then
    echo "[ERROR] arm64 kernel build dir not found: $KDIR"
    echo "[HINT ] 请先在 $KERNEL_DIR/src 下编译 arm64 内核"
    exit 1
fi

if [ ! -f "$KERNEL_IMG" ] && [ -f "$KDIR/arch/arm64/boot/Image" ]; then
    mkdir -p "$(dirname "$KERNEL_IMG")"
    cp "$KDIR/arch/arm64/boot/Image" "$KERNEL_IMG"
fi

if [ ! -f "$KERNEL_IMG" ]; then
    echo "[ERROR] arm64 kernel Image not found: $KERNEL_IMG"
    echo "[HINT ] 先执行 arm64 内核编译，并确认 Image 已经生成"
    exit 1
fi

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

if [ -z "$BUSYBOX_PATH" ]; then
    echo "[ERROR] arm64 busybox not found under: $BUSYBOX_DIR"
    echo "[HINT ] 请先在 $BUSYBOX_DIR/src 下编译并安装 arm64 busybox"
    exit 1
fi

echo "[INFO] KERNEL_DIR          : $KERNEL_DIR"
echo "[INFO] BUSYBOX_DIR         : $BUSYBOX_DIR"
echo "[INFO] Kernel build dir    : $KDIR"
echo "[INFO] Kernel image        : $KERNEL_IMG"
echo "[INFO] BusyBox             : $BUSYBOX_PATH"
echo "[INFO] CROSS_COMPILE       : $CROSS_COMPILE"
echo "[INFO] QEMU                : $QEMU_BIN"
BUSYBOX_FILE_INFO=$(file "$BUSYBOX_PATH" || true)
echo "$BUSYBOX_FILE_INFO"
if echo "$BUSYBOX_FILE_INFO" | grep -q "dynamically linked"; then
    echo "[ERROR] 当前 busybox 是动态链接版，不能直接用于最小 initramfs/rootfs。"
    echo "[ERROR] 请先在 $BUSYBOX_DIR/src 下重新编译静态链接 BusyBox。"
    exit 1
fi

make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" clean
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE"

rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,sbin,etc,tmp}

cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"

for cmd in sh ls cat echo mount insmod rmmod dmesg chmod sleep ps mkdir grep date rm touch sync uname poweroff reboot tail head; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

cp demo_of.ko "$ROOTFS/"

cat > "$ROOTFS/init" <<'EOT'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

echo "==============================================="
echo " Linux Driver Lab Day09 Device Tree init ready"
echo "==============================================="
echo
echo "[Guest Tips]"
echo "  加载模块           : insmod /demo_of.ko"
echo "  卸载模块           : rmmod demo_of"
echo "  查看日志           : dmesg | grep demo_of"
echo "  查看 DT 节点       : ls /sys/firmware/devicetree/base"
echo "  查看平台设备       : ls /sys/bus/platform/devices"
echo "  查看平台驱动       : ls /sys/bus/platform/drivers"
echo

exec /bin/sh
EOT

chmod +x "$ROOTFS/init"

(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)

"$QEMU_BIN"     -machine virt,dumpdtb=virt-base.dtb     -cpu cortex-a57     -m 512     -nographic     >/dev/null 2>&1 || true

if [ ! -f virt-base.dtb ]; then
    echo "[ERROR] failed to dump QEMU virt base DTB"
    exit 1
fi

"$DTC_BIN" -I dtb -O dts -o virt-base.dts virt-base.dtb

"$PYTHON_BIN" ./inject_virt_dt.py     --input virt-base.dts     --fragment demo_day09.fragment.dtsi     --output virt-day09.dts

"$DTC_BIN" -I dts -O dtb -o virt-day09.dtb virt-day09.dts

"$QEMU_BIN"     -machine virt     -cpu cortex-a57     -m 1024     -nographic     -kernel "$KERNEL_IMG"     -dtb virt-day09.dtb     -initrd rootfs.img     -append "console=ttyAMA0 root=/dev/ram0 rw rdinit=/init"
