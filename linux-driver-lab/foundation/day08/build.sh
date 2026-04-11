#!/bin/bash
set -e

#
# Day08 build.sh 负责三件事
# 1. 编译 platform_driver 模块 demo_pdrv.ko
# 2. 组装一个最小 rootfs
# 3. 启动 QEMU 进入 guest 验证 probe/remove 生命周期
#
# 这一版有意不放用户态 test 程序
# 因为 Day08 的重点不是 /dev 接口
# 而是 platform bus 的匹配和 devm 资源管理
#

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

# 新路径：
#   ../kernel-src/linux-5.15.10/build/x86
#   ../kernel-src/linux-5.15.10/output/x86/bzImage
#   ../kernel-src/busybox-1.36.1/output/x86/_install
# 旧绝对路径示例：
#   /home/wq7/workspace/kernel-src/linux-5.15.10/build/x86
#   /home/wq7/workspace/kernel-src/linux-5.15.10/output/x86/bzImage
#   /home/wq7/workspace/kernel-src/busybox-1.36.1/output/x86/_install
KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../../kernel-src/linux-5.15.10/build/x86}"
KERNEL_IMG="${KERNEL_IMG:-$REPO_ROOT/../../kernel-src/linux-5.15.10/output/x86/bzImage}"
BUSYBOX_INSTALL="${BUSYBOX_INSTALL:-$REPO_ROOT/../../kernel-src/busybox-1.36.1/output/x86/_install}"
BUSYBOX_DIR="${BUSYBOX_DIR:-$REPO_ROOT/../../kernel-src/busybox-1.36.1/output/x86}"
ROOTFS="./rootfs"

BUSYBOX_CANDIDATES=(
    "$BUSYBOX_INSTALL/bin/busybox"
    "$BUSYBOX_DIR/busybox"
)

BUSYBOX_PATH=""
for p in "${BUSYBOX_CANDIDATES[@]}"; do
    if [ -x "$p" ]; then
        BUSYBOX_PATH="$p"
        break
    fi
done

if [ ! -d "$KERNEL_DIR" ]; then
    echo "[ERROR] kernel build dir not found: $KERNEL_DIR"
    exit 1
fi

if [ ! -f "$KERNEL_IMG" ]; then
    echo "[ERROR] kernel image not found: $KERNEL_IMG"
    exit 1
fi

if [ -z "$BUSYBOX_PATH" ]; then
    echo "[ERROR] busybox not found in: $BUSYBOX_INSTALL or $BUSYBOX_DIR"
    exit 1
fi

echo "[INFO] Using kernel build dir : $KERNEL_DIR"
echo "[INFO] Using kernel image     : $KERNEL_IMG"
echo "[INFO] Using busybox          : $BUSYBOX_PATH"
BUSYBOX_FILE_INFO=$(file "$BUSYBOX_PATH" || true)
echo "$BUSYBOX_FILE_INFO"
if echo "$BUSYBOX_FILE_INFO" | grep -q "dynamically linked"; then
    echo "[ERROR] 当前 busybox 是动态链接版，不能直接用于最小 initramfs/rootfs。"
    echo "[ERROR] 请先在 kernel-src/busybox-1.36.1/src 下重新编译静态链接 BusyBox。"
    exit 1
fi


make KDIR="$KERNEL_DIR" clean
make KDIR="$KERNEL_DIR"

rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,sbin,etc,tmp}

cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"

for cmd in sh ls cat echo mount insmod rmmod dmesg chmod sleep ps mkdir grep date rm touch sync uname poweroff reboot tail head; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

cp demo_pdrv.ko "$ROOTFS/"

cat > "$ROOTFS/init" <<'EOT'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

echo "============================================"
echo " Linux Driver Lab Day08 platform init ready"
echo "============================================"
echo
echo "[Guest Tips]"
echo "  加载模块      : insmod /demo_pdrv.ko"
echo "  卸载模块      : rmmod demo_pdrv"
echo "  查看日志      : dmesg | tail -n 50"
echo "  查看平台设备  : ls /sys/bus/platform/devices"
echo "  查看平台驱动  : ls /sys/bus/platform/drivers"
echo

exec /bin/sh
EOT

chmod +x "$ROOTFS/init"

(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)

qemu-system-x86_64 \
    -kernel "$KERNEL_IMG" \
    -initrd rootfs.img \
    -nographic \
    -append "console=ttyS0 noapic root=/dev/ram0 rw rdinit=/init"
