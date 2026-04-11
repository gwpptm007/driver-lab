#!/bin/bash
set -e

# Day11 目标
# 1. 在 day10 request_irq + top-half 的基础上接入 workqueue
# 2. top-half 只做“记账 + queue_work”，不在中断上下文里做重活
# 3. 在 worker 中模拟耗时处理，观察“重活下沉”后的执行路径
# 4. 把从 IRQ 进入到 worker 开始运行之间的粗略延迟导出到 /proc
#
# 建议先导出下面几个变量：
#   export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
#   export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
#   export CROSS_COMPILE=aarch64-linux-gnu-
#   ./build.sh

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

# 允许外部覆盖，便于你后面迁移目录或复用这套脚本
KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/../../kernel-src/linux-5.15.10}"
BUSYBOX_DIR="${BUSYBOX_DIR:-$REPO_ROOT/../../kernel-src/busybox-1.36.1}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
ARCH_NAME="${ARCH_NAME:-arm64}"
QEMU_BIN="${QEMU_BIN:-qemu-system-aarch64}"
DTC_BIN="${DTC_BIN:-dtc}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
ROOTFS="./rootfs"

# 内核 / BusyBox 的常用输出路径
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

# 兼容两种 Image 位置：
# 1. 你手工整理后的 output/arm64/Image
# 2. 内核构建目录里的 arch/arm64/boot/Image
if [ ! -f "$KERNEL_IMG" ] && [ -f "$KDIR/arch/arm64/boot/Image" ]; then
    mkdir -p "$(dirname "$KERNEL_IMG")"
    cp "$KDIR/arch/arm64/boot/Image" "$KERNEL_IMG"
fi

if [ ! -f "$KERNEL_IMG" ]; then
    echo "[ERROR] arm64 kernel Image not found: $KERNEL_IMG"
    echo "[HINT ] 先执行 arm64 内核编译 并确认 Image 已经生成"
    exit 1
fi

# BusyBox 这里保留多候选路径，方便兼容你前面的实验目录形态
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

# 最小 initramfs / rootfs 更适合静态 BusyBox
BUSYBOX_FILE_INFO=$(file "$BUSYBOX_PATH" || true)
echo "$BUSYBOX_FILE_INFO"
if echo "$BUSYBOX_FILE_INFO" | grep -q "dynamically linked"; then
    echo "[ERROR] 当前 busybox 是动态链接版 不能直接用于最小 initramfs 或 rootfs"
    echo "[ERROR] 请先在 $BUSYBOX_DIR/src 下重新编译静态链接 BusyBox"
    exit 1
fi

# 第 1 阶段：编译 day11 模块
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" clean
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE"

# 第 2 阶段：构造最小 rootfs
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,sbin,etc,tmp}

cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"

# 给常用命令创建 BusyBox 软链接，方便 guest 内直接测试
for cmd in sh ls cat echo mount insmod rmmod dmesg chmod sleep ps mkdir grep date rm touch sync uname poweroff reboot tail head usleep; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

cp demo_irq_wq.ko "$ROOTFS/"

# 第 3 阶段：生成 guest /init
# 这里把 day11 的验收命令直接印在开机提示里，方便你上手测试
cat > "$ROOTFS/init" <<'EOT'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

echo "==========================================================="
echo " Linux Driver Lab Day11 IRQ top-half + workqueue init ready"
echo "==========================================================="
echo
echo "[Guest Tips]"
echo "  加载模块              : insmod /demo_irq_wq.ko"
echo "  指定模拟重活 50ms     : insmod /demo_irq_wq.ko work_ms=50"
echo "  查看驱动日志          : dmesg | grep demo_irq_wq"
echo "  查看 IRQ 统计         : cat /proc/interrupts | grep demo_irq_wq"
echo "  查看内部统计          : cat /proc/demo_irq_wq_stats"
echo "  触发 1 次中断         : echo 1 > /proc/demo_irq_wq_trigger"
echo "  触发 10 次中断        : echo 10 > /proc/demo_irq_wq_trigger"
echo "  卸载模块              : rmmod demo_irq_wq"
echo

exec /bin/sh
EOT

chmod +x "$ROOTFS/init"
(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)

# 第 4 阶段：让 QEMU 导出一份基础 virt DTB
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

# 第 5 阶段：dtb -> dts，注入 day11 的教学节点，再编回 dtb
"$DTC_BIN" -I dtb -O dts -o virt-base.dts virt-base.dtb
"$PYTHON_BIN" ./inject_virt_dt.py \
    --input virt-base.dts \
    --fragment demo_irq_wq.fragment.dtsi \
    --output virt-day11.dts
"$DTC_BIN" -I dts -O dtb -o virt-day11.dtb virt-day11.dts

# 第 6 阶段：启动 QEMU
# 这里真正启动的是：
# - arm64 内核镜像 Image
# - 含 day11 fake 设备节点的 DTB
# - rootfs.img
# 进入 guest 后就可以按提示 insmod / 测试 / 看统计
"$QEMU_BIN" \
    -machine virt \
    -cpu cortex-a57 \
    -m 1024 \
    -nographic \
    -kernel "$KERNEL_IMG" \
    -dtb virt-day11.dtb \
    -initrd rootfs.img \
    -append "console=ttyAMA0 root=/dev/ram0 rw rdinit=/init"
