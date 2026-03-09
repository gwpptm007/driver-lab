#!/bin/bash
set -e

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
KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10/build/x86}"
KERNEL_IMG="${KERNEL_IMG:-$REPO_ROOT/../kernel-src/linux-5.15.10/output/x86/bzImage}"
BUSYBOX_INSTALL="${BUSYBOX_INSTALL:-$REPO_ROOT/../kernel-src/busybox-1.36.1/output/x86/_install}"
BUSYBOX_DIR="${BUSYBOX_DIR:-$REPO_ROOT/../kernel-src/busybox-1.36.1/output/x86}"
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

if [ -z "$BUSYBOX_PATH" ]; then
    echo "[ERROR] busybox not found in: $BUSYBOX_DIR"
    exit 1
fi

chmod +x build.sh

echo "[INFO] Using kernel : $KERNEL_DIR"
echo "[INFO] Using busybox: $BUSYBOX_PATH"
BUSYBOX_FILE_INFO=$(file "$BUSYBOX_PATH" || true)
echo "$BUSYBOX_FILE_INFO"
if echo "$BUSYBOX_FILE_INFO" | grep -q "dynamically linked"; then
    echo "[ERROR] 当前 busybox 是动态链接版，不能直接用于最小 initramfs/rootfs。"
    echo "[ERROR] 请先在 kernel-src/busybox-1.36.1/src 下重新编译静态链接 BusyBox。"
    exit 1
fi

# 这里显式把 KDIR 传给 Makefile。
# build.sh 里我习惯把变量叫 KERNEL_DIR，但内核模块 Makefile 里常用 KDIR。
# 所以这里把两者对应起来，避免路径解析只停留在 shell 变量里。
make KDIR="$KERNEL_DIR" clean
make KDIR="$KERNEL_DIR"

rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,sbin,etc,tmp}
mkdir -p "$ROOTFS/sys/kernel/debug"

cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"

for cmd in sh ls cat cp echo mount insmod rmmod dmesg chmod sleep ps mkdir grep date rm touch sync uname poweroff reboot tail head; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

cp demo.ko "$ROOTFS/"
cp test "$ROOTFS/bin/"
chmod +x "$ROOTFS/bin/test"

cat > "$ROOTFS/init" <<'EOT'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t debugfs debugfs /sys/kernel/debug

echo "========================================"
echo " Linux Driver Lab Day05 init is running"
echo "========================================"

insmod /demo.ko || echo "insmod demo.ko failed"

echo
echo "[Guest Tips]"
echo "  read block test : /bin/test read"
echo "  write data      : /bin/test write hello"
echo "  ioctl set/get   : /bin/test set 123 ; /bin/test get"
echo "  sysfs           : cat /sys/class/demo_class/demo/enable"
echo "  debugfs         : cat /sys/kernel/debug/demo_debug/status"
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
