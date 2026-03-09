#!/bin/bash
set -e

#
# build.sh 负责三件事：
# 1. 编译内核模块 demo.ko 和用户态 test 程序
# 2. 组装一个最小 rootfs，把脚本/模块/test 放进去
# 3. 直接启动 qemu，方便你立刻进 guest 测试
#
# 这类“构建 + 打包 rootfs + 启动虚拟机”的流程，在驱动实验里很常见。
# 这样可以把实验环境固定下来，避免每次手工准备 rootfs。
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
KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../kernel-src/linux-5.15.10/build/x86}"
KERNEL_IMG="${KERNEL_IMG:-$REPO_ROOT/../kernel-src/linux-5.15.10/output/x86/bzImage}"
BUSYBOX_INSTALL="${BUSYBOX_INSTALL:-$REPO_ROOT/../kernel-src/busybox-1.36.1/output/x86/_install}"
BUSYBOX_DIR="${BUSYBOX_DIR:-$REPO_ROOT/../kernel-src/busybox-1.36.1/output/x86}"
ROOTFS="./rootfs"

#
# BusyBox 可能有两种常见位置：
# 1. make install 后的 _install/bin/busybox
# 2. 源码目录直接生成的 busybox
#
# 按顺序尝试，找到能执行的那个就用它
#
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

# 编译模块和用户态测试程序。
# 这里显式把 KDIR 传给 Makefile。
# build.sh 里我习惯把变量叫 KERNEL_DIR，但内核模块 Makefile 里常用 KDIR。
# 所以这里把两者对应起来，避免路径解析只停留在 shell 变量里。
make KDIR="$KERNEL_DIR" clean
make KDIR="$KERNEL_DIR"

# 重建最小 rootfs。
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,sbin,etc,tmp}
mkdir -p "$ROOTFS/sys/kernel/debug"

cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"

#
# BusyBox 通过“一个可执行文件 + 多个软链接”的方式复用命令
# 把实验里常用到的命令都链接出来。
#
for cmd in sh ls cat cp echo mount insmod rmmod dmesg chmod sleep ps mkdir grep date rm touch sync uname poweroff reboot tail head; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

# 拷贝模块与测试程序
cp demo.ko "$ROOTFS/"
cp test "$ROOTFS/bin/"
chmod +x "$ROOTFS/bin/test"

# 拷贝 Day06 的 4 个验收脚本
for script in insmod_rmmod.sh stress_rw.sh check_dmesg.sh all.sh; do
    cp "$script" "$ROOTFS/bin/"
    chmod +x "$ROOTFS/bin/$script"
done

#
# 生成 guest 的 /init。
#
# init 驱动实验：
# - 挂 proc/sys/devtmpfs/debugfs
# - 自动加载 demo.ko
# - 打印常用测试命令提示
# - 最后进入 shell
#
cat > "$ROOTFS/init" <<'EOT'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t debugfs debugfs /sys/kernel/debug

echo "========================================"
echo " Linux Driver Lab Day06 init is running"
echo "========================================"

insmod /demo.ko || echo "insmod demo.ko failed"

echo
echo "[Guest Tips]"
echo "  一键验收      : /bin/all.sh"
echo "  回归 500 次   : /bin/insmod_rmmod.sh 500"
echo "  并发 5 分钟   : /bin/stress_rw.sh 300"
echo "  dmesg 检查    : /bin/check_dmesg.sh"
echo "  debugfs 状态  : cat /sys/kernel/debug/demo_debug/status"
echo

exec /bin/sh
EOT

chmod +x "$ROOTFS/init"

# 打包成 initramfs(rootfs.img)。
(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)

# 启动 QEMU
qemu-system-x86_64 \
    -kernel "$KERNEL_IMG" \
    -initrd rootfs.img \
    -nographic \
    -append "console=ttyS0 noapic root=/dev/ram0 rw rdinit=/init"
