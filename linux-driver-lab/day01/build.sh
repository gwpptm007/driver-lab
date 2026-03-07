#!/bin/bash
set -e

# ============================================================
# Linux Driver Lab - build.sh
# 作用：
#   1. 编译当前 day 的驱动模块
#   2. 如有需要，编译用户态测试程序
#   3. 准备最小 rootfs
#   4. 拷贝 busybox、demo.ko、测试程序
#   5. 创建 /init 脚本
#   6. 打包 rootfs.img
#   7. 启动 QEMU
#
# 初学阶段建议每次都先执行：
#   chmod +x build.sh
#   ./build.sh
# ============================================================

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

# 当前推荐目录结构：
# driver-lab/
# ├── kernel-src/
# │   ├── linux-5.15.10/
# │   └── busybox-1.36.1/
# └── linux-driver-lab/
#
# 所以这里优先走仓库相对路径：
#   ../kernel-src/linux-5.15.10
#   ../kernel-src/busybox-1.36.1
DEFAULT_KERNEL_BASE="$REPO_ROOT/../kernel-src"

# 历史上我自己的环境曾经直接写死在：
#   /home/wq7/workspace/kernel-src/linux-5.15.10
#   /home/wq7/workspace/kernel-src/busybox-1.36.1
# 现在不再默认写死使用，但保留为兼容回退路径，方便理解旧版本脚本。
LEGACY_KERNEL_BASE="/home/wq7/workspace/kernel-src"

KDIR="${KDIR:-$DEFAULT_KERNEL_BASE/linux-5.15.10}"
BUSYBOX_DIR="${BUSYBOX_DIR:-$DEFAULT_KERNEL_BASE/busybox-1.36.1}"

if [ ! -d "$KDIR" ] && [ -d "$LEGACY_KERNEL_BASE/linux-5.15.10" ]; then
    KDIR="$LEGACY_KERNEL_BASE/linux-5.15.10"
fi

if [ ! -d "$BUSYBOX_DIR" ] && [ -d "$LEGACY_KERNEL_BASE/busybox-1.36.1" ]; then
    BUSYBOX_DIR="$LEGACY_KERNEL_BASE/busybox-1.36.1"
fi
KERNEL_IMG="$KDIR/arch/x86/boot/bzImage"
CUR_DIR=$(pwd)
ROOTFS="$CUR_DIR/rootfs"

# 优先使用自己编译的 busybox，而不是宿主机 /usr/bin/busybox。
# 原因：宿主机版本可能是动态链接版，在最小 rootfs 里会执行失败。
BUSYBOX_CANDIDATES=(
    "$BUSYBOX_DIR/_install/bin/busybox"
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
    echo "[ERROR] 未找到可执行的 busybox。"
    echo "[ERROR] 请先确认 $BUSYBOX_DIR 已正确编译。"
    exit 1
fi

echo "[INFO] Using kernel image: $KERNEL_IMG"
echo "[INFO] Using busybox: $BUSYBOX_PATH"
file "$BUSYBOX_PATH" || true

echo "[STEP 1] 编译驱动模块"
# 这里显式把 KDIR 传给 Makefile。
# 即使 Makefile 里保留默认值，build.sh 也会优先使用这里解析出来的路径。
make KDIR="$KDIR" clean
make KDIR="$KDIR"

# 如果当前目录里有额外的用户态测试程序，则在后续自动编译 / 复制。
if [ -f test.c ]; then
    echo "[STEP 1.1] 编译用户态测试程序 test.c"
    gcc -static test.c -o test
fi

if [ -f test_ioctl.c ]; then
    echo "[STEP 1.1] 编译用户态测试程序 test_ioctl.c"
    gcc -static test_ioctl.c -o test_ioctl
fi

echo "[STEP 2] 重新准备 rootfs，避免旧文件污染"
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,etc,sbin,tmp}

# 拷贝 busybox，并创建常用命令软链接。
cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"
for cmd in sh ls cat cp echo mount insmod rmmod dmesg mkdir uname; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

# 拷贝驱动产物
if [ -f demo.ko ]; then
    cp demo.ko "$ROOTFS/"
fi

# 拷贝测试程序（如果存在）
if [ -f test ]; then
    cp test "$ROOTFS/bin/"
    chmod +x "$ROOTFS/bin/test"
fi

if [ -f test_ioctl ]; then
    cp test_ioctl "$ROOTFS/bin/"
    chmod +x "$ROOTFS/bin/test_ioctl"
fi

# 某些 day 需要 debugfs；这里统一先不挂，按目录判断。
MOUNT_DEBUGFS="0"
if [ -f demo_ioctl.h ]; then
    :
fi
if [[ "$CUR_DIR" == *"day04"* ]]; then
    MOUNT_DEBUGFS="1"
fi

echo "[STEP 3] 创建 /init 启动脚本"
cat > "$ROOTFS/init" <<INITEOF
#!/bin/sh
# /init 是最小 Linux 用户空间启动后的第一个脚本。
# 它负责挂载基础文件系统、加载驱动、进入 shell。

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
INITEOF

if [ "$MOUNT_DEBUGFS" = "1" ]; then
cat >> "$ROOTFS/init" <<'INITEOF'
mount -t debugfs none /sys/kernel/debug
INITEOF
fi

cat >> "$ROOTFS/init" <<'INITEOF'
echo "========================================"
echo " Linux Driver Lab init is running..."
echo "========================================"

if [ -f /demo.ko ]; then
    insmod /demo.ko || echo "[WARN] insmod /demo.ko failed"
fi

exec /bin/sh
INITEOF

# 给 /init 增加执行权限，避免出现 “/init exists but couldn't execute” 一类问题。
chmod +x "$ROOTFS/init"

echo "[STEP 4] 打包 rootfs.img"
(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)

echo "[STEP 5] 启动 QEMU"
echo "[TIP] 退出 QEMU: Ctrl+a x ；QEMU monitor: Ctrl+a c 后输入 quit"
qemu-system-x86_64 \
    -kernel "$KERNEL_IMG" \
    -initrd rootfs.img \
    -nographic \
    -append "console=ttyS0 noapic root=/dev/ram0 rw rdinit=/init"
