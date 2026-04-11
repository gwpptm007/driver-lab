#!/bin/bash
set -e

# -----------------------------------------------------------------------------
# Day10 build.sh
#
# 这个脚本负责把 day10 的整个实验链路一次性串起来：
#
# 1. 编译 arm64 外部模块 demo_irq.ko
# 2. 构造最小 rootfs，并把 BusyBox + 模块放进去
# 3. 先让 QEMU 导出 virt 机器原始 DTB
# 4. 把原始 DTB 反编译成 DTS
# 5. 把 day10 的教学 DT 片段注入到基础 DTS 中
# 6. 再把新 DTS 编译回新的 DTB
# 7. 用内核 Image + 新 DTB + rootfs.img 启动 ARM64 QEMU
#
# 这样你最终进入的 guest 环境，就是一个专门为 day10 IRQ 教学实验准备的
# 最小系统：
#
#   内核     -> 你自己编出来的 arm64 Image
#   设备树   -> QEMU virt 基础 DT + day10 fake 设备节点
#   用户空间 -> BusyBox 构造的最小 rootfs
# -----------------------------------------------------------------------------

#
# Day10 实验目标回顾：
# - 在 day09 的 DT platform driver 基础上接入 request_irq()
# - 注册一个最小 top-half handler
# - 通过 /proc/demo_irq_trigger 做软件注入
# - 使用 /proc/interrupts + /proc/demo_irq_stats 验证计数增长
#

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

# -----------------------------------------------------------------------------
# 下面这些变量都允许从外部覆盖。
#
# 好处是：
# - 换内核版本时，不用改脚本正文
# - 换 busybox 目录时，不用改脚本正文
# - 换交叉编译器前缀时，也不用改脚本正文
#
# 例如：
#   export KERNEL_DIR=/path/to/linux-5.15.10
#   export BUSYBOX_DIR=/path/to/busybox-1.36.1
#   export CROSS_COMPILE=aarch64-linux-gnu-
#   ./build.sh
# -----------------------------------------------------------------------------
KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../../kernel-src/linux-5.15.10}"
BUSYBOX_DIR="${BUSYBOX_DIR:-$REPO_ROOT/../../kernel-src/busybox-1.36.1}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
ARCH_NAME="${ARCH_NAME:-arm64}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
DTC_BIN="${DTC_BIN:-dtc}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
ROOTFS="./rootfs"

# KDIR 指向 arm64 内核构建目录（也就是 make O=... 使用的输出目录）
KDIR="${KDIR:-$KERNEL_DIR/build/arm64}"

# KERNEL_IMG 指向最终要交给 QEMU 启动的 Image
KERNEL_IMG="${KERNEL_IMG:-$KERNEL_DIR/output/arm64/Image}"

# BusyBox 安装输出目录，优先从这里找 _install/bin/busybox
BUSYBOX_INSTALL="${BUSYBOX_INSTALL:-$BUSYBOX_DIR/output/arm64/_install}"

# BusyBox 普通输出目录，兼容某些尚未执行 install 的场景
BUSYBOX_OUTPUT="${BUSYBOX_OUTPUT:-$BUSYBOX_DIR/output/arm64}"

# -----------------------------------------------------------------------------
# 第 1 段：检查内核/BusyBox 根目录是否存在
# -----------------------------------------------------------------------------
if [ ! -d "$KERNEL_DIR" ]; then
    echo "[ERROR] KERNEL_DIR not found: $KERNEL_DIR"
    exit 1
fi

if [ ! -d "$BUSYBOX_DIR" ]; then
    echo "[ERROR] BUSYBOX_DIR not found: $BUSYBOX_DIR"
    exit 1
fi

# -----------------------------------------------------------------------------
# 第 2 段：检查宿主机依赖工具是否齐全
# -----------------------------------------------------------------------------
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

# -----------------------------------------------------------------------------
# 第 3 段：检查 arm64 内核构建目录与 Image 是否存在
# -----------------------------------------------------------------------------
if [ ! -d "$KDIR" ]; then
    echo "[ERROR] arm64 kernel build dir not found: $KDIR"
    echo "[HINT ] 请先在 $KERNEL_DIR/src 下编译 arm64 内核"
    exit 1
fi

# 如果 output/arm64/Image 还没同步，但 build 目录里的 Image 已经存在，
# 就自动补一次复制，减少手工步骤。
if [ ! -f "$KERNEL_IMG" ] && [ -f "$KDIR/arch/arm64/boot/Image" ]; then
    mkdir -p "$(dirname "$KERNEL_IMG")"
    cp "$KDIR/arch/arm64/boot/Image" "$KERNEL_IMG"
fi

if [ ! -f "$KERNEL_IMG" ]; then
    echo "[ERROR] arm64 kernel Image not found: $KERNEL_IMG"
    echo "[HINT ] 先执行 arm64 内核编译 并确认 Image 已经生成"
    exit 1
fi

# -----------------------------------------------------------------------------
# 第 4 段：从多个候选路径中寻找可用的 arm64 BusyBox
# -----------------------------------------------------------------------------
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

# 打印最终使用的关键路径，方便你核对实验环境。
echo "[INFO] KERNEL_DIR          : $KERNEL_DIR"
echo "[INFO] BUSYBOX_DIR         : $BUSYBOX_DIR"
echo "[INFO] Kernel build dir    : $KDIR"
echo "[INFO] Kernel image        : $KERNEL_IMG"
echo "[INFO] BusyBox             : $BUSYBOX_PATH"
echo "[INFO] CROSS_COMPILE       : $CROSS_COMPILE"
echo "[INFO] QEMU                : $QEMU_BIN"

# BusyBox 必须是静态链接版。
# 否则把它直接拷到最小 rootfs 里之后，guest 往往会因为缺共享库而无法运行。
BUSYBOX_FILE_INFO=$(file "$BUSYBOX_PATH" || true)
echo "$BUSYBOX_FILE_INFO"
if echo "$BUSYBOX_FILE_INFO" | grep -q "dynamically linked"; then
    echo "[ERROR] 当前 busybox 是动态链接版 不能直接用于最小 initramfs 或 rootfs"
    echo "[ERROR] 请先在 $BUSYBOX_DIR/src 下重新编译静态链接 BusyBox"
    exit 1
fi

# -----------------------------------------------------------------------------
# 第 5 段：编译 day10 模块
# -----------------------------------------------------------------------------
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" clean
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE"

# -----------------------------------------------------------------------------
# 第 6 段：构造最小 rootfs
# -----------------------------------------------------------------------------
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,sbin,etc,tmp}

# 放入 BusyBox 主程序
cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"

# 给常用命令建立软链接。这样 guest 里能直接使用这些命令。
for cmd in sh ls cat echo mount insmod rmmod dmesg chmod sleep ps mkdir grep date rm touch sync uname poweroff reboot tail head; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

# 把本课要加载的模块直接拷到 guest 根目录，进入 guest 后可以直接 insmod /demo_irq.ko
cp demo_irq.ko "$ROOTFS/"

# 生成最小 init 脚本。
# guest 启动后会先执行这里：挂载 proc/sys/dev，然后提示你如何测试，再进入 shell。
cat > "$ROOTFS/init" <<'EOT'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

echo "===================================================="
echo " Linux Driver Lab Day10 IRQ top-half init ready"
echo "===================================================="
echo
echo "[Guest Tips]"
echo "  加载模块           : insmod /demo_irq.ko"
echo "  查看驱动日志       : dmesg | grep demo_irq"
echo "  查看中断统计       : cat /proc/interrupts | grep demo_irq"
echo "  查看内部统计       : cat /proc/demo_irq_stats"
echo "  触发 1 次中断      : echo 1 > /proc/demo_irq_trigger"
echo "  触发 10 次中断     : echo 10 > /proc/demo_irq_trigger"
echo "  卸载模块           : rmmod demo_irq"
echo

exec /bin/sh
EOT

chmod +x "$ROOTFS/init"

# 把 rootfs 目录打包成 initramfs 镜像 rootfs.img。
(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)

# -----------------------------------------------------------------------------
# 第 7 段：先导出 QEMU virt 的基础 DTB
# -----------------------------------------------------------------------------
# 这里不是正式启动 guest，而是借 QEMU 自己把 virt 机器的默认 DTB 导出来。
# 后面我们会把 day10 的 fake 设备片段注入进去。
"$QEMU_BIN" \
    -machine virt,dumpdtb=virt-base.dtb \
    -cpu cortex-a57 \
    -m 512 \
    -nographic \
    >/dev/null 2>&1 || true

if [ ! -f virt-base.dtb ]; then
    echo "[ERROR] failed to dump QEMU virt base DTB"
    exit 1
fi

# 反编译为可读的 DTS，便于注入教学节点。
"$DTC_BIN" -I dtb -O dts -o virt-base.dts virt-base.dtb

# -----------------------------------------------------------------------------
# 第 8 段：注入 day10 教学 DT 片段，再编译回新的 DTB
# -----------------------------------------------------------------------------
"$PYTHON_BIN" ./inject_virt_dt.py \
    --input virt-base.dts \
    --fragment demo_irq.fragment.dtsi \
    --output virt-irq.dts

"$DTC_BIN" -I dts -O dtb -o virt-irq.dtb virt-irq.dts

# -----------------------------------------------------------------------------
# 第 9 段：正式启动 ARM64 QEMU
# -----------------------------------------------------------------------------
# 最终 guest 的三个关键输入分别是：
# - kernel : 你自己的 arm64 Image
# - dtb    : 注入了 day10 fake 设备节点的新 DTB
# - initrd : 刚刚打好的最小 rootfs.img
"$QEMU_BIN" \
    -machine virt \
    -cpu cortex-a57 \
    -m 1024 \
    -nographic \
    -kernel "$KERNEL_IMG" \
    -dtb virt-irq.dtb \
    -initrd rootfs.img \
    -append "console=ttyAMA0 root=/dev/ram0 rw rdinit=/init"
