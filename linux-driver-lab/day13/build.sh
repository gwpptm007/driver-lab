#!/bin/bash
set -e

# Day13 build.sh 说明
# -------------------
# 这个脚本的职责不是只“编译模块”，而是一次性把完整教学环境拼出来：
#
# 1. 编译 day13 模块 demo_regmap.ko
# 2. 构造最小 arm64 rootfs
# 3. 把 demo_regmap.ko 和 guest 内 trace/归档脚本打进 rootfs
# 4. 导出 QEMU virt 基础 DTB
# 5. 向 DTS 注入 day13 的教学节点
# 6. 再打回 DTB 并启动 arm64 QEMU
#
# 这样做的目的：
# - 避免你每次手工准备 rootfs
# - 保持 day09/day10/day11/day12/day13 的实验方式一致
# - 让“编译 -> 启动 -> 进入 guest 测试”形成固定节奏
#
# 推荐先导出：
#   export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
#   export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
#   export CROSS_COMPILE=aarch64-linux-gnu-
#   ./build.sh
#
# 经验结论：
# - ARM 场景里更常见三者都要配；
# - 非 ARM 场景下，前两个可能仍然要配，第三个则看你是不是交叉编译。

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
    echo "[HINT ] 请先在 $KERNEL_DIR 下完成 arm64 内核编译"
    exit 1
fi

if [ ! -f "$KERNEL_IMG" ] && [ -f "$KDIR/arch/arm64/boot/Image" ]; then
    mkdir -p "$(dirname "$KERNEL_IMG")"
    cp "$KDIR/arch/arm64/boot/Image" "$KERNEL_IMG"
fi

if [ ! -f "$KERNEL_IMG" ]; then
    echo "[ERROR] arm64 kernel Image not found: $KERNEL_IMG"
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
    echo "[ERROR] 当前 busybox 是动态链接版，不能直接用于最小 initramfs/rootfs"
    exit 1
fi

make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" clean
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE"

rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,dev,proc,sys,sbin,etc,tmp}
mkdir -p "$ROOTFS/sys/kernel/debug"

cp "$BUSYBOX_PATH" "$ROOTFS/bin/busybox"
chmod +x "$ROOTFS/bin/busybox"

for cmd in sh ls cat echo mount insmod rmmod dmesg chmod sleep ps mkdir grep date rm touch sync uname poweroff reboot tail head usleep cp wc sed; do
    ln -sf busybox "$ROOTFS/bin/$cmd"
done

cp demo_regmap.ko "$ROOTFS/"
cp guest_trace_irq_path.sh "$ROOTFS/bin/day13_trace_irq_path.sh"
cp guest_archive_trace.sh "$ROOTFS/bin/day13_archive_trace.sh"
# Day15 的 guest_collect.sh 也一起打进 rootfs。这样后面做 baseline 采样时，
# 宿主机脚本就可以通过串口直接调用 /bin/day15_guest_collect.sh，
# 不需要你在 guest 里再手工拷脚本。
cp ../day15/collect/guest_collect.sh "$ROOTFS/bin/day15_guest_collect.sh"
cp function_graph_targets.txt "$ROOTFS/etc/day13_function_graph_targets.txt"
chmod +x "$ROOTFS/bin/day13_trace_irq_path.sh" "$ROOTFS/bin/day13_archive_trace.sh" "$ROOTFS/bin/day15_guest_collect.sh"

cat > "$ROOTFS/init" <<'EOT'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t debugfs debugfs /sys/kernel/debug

echo "==============================================================="
echo " Linux Driver Lab Day13 function_graph IRQ path tracing ready"
echo "==============================================================="
echo "1) insmod /demo_regmap.ko"
echo "2) cat /sys/kernel/debug/demo_regmap/snapshot"
echo "3) /bin/day13_trace_irq_path.sh 1"
echo "4) cat /tmp/day13_irq_function_graph.txt"
echo "5) /bin/day13_archive_trace.sh"
echo "6) /bin/day15_guest_collect.sh   # Day15 baseline 采样"
echo "==============================================================="
exec /bin/sh
EOT
chmod +x "$ROOTFS/init"

(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)

rm -f virt-base.dtb virt-base.dts virt-day13.dts virt-day13.dtb
"$QEMU_BIN"     -machine virt,dumpdtb=virt-base.dtb     -cpu cortex-a57     -m 512     -nographic     >/dev/null 2>&1 || true

if [ ! -f virt-base.dtb ]; then
    echo "[ERROR] failed to dump QEMU virt base DTB"
    exit 1
fi

"$DTC_BIN" -I dtb -O dts -o virt-base.dts virt-base.dtb
"$PYTHON_BIN" inject_virt_dt.py     --input virt-base.dts     --fragment demo_regmap.fragment.dtsi     --output virt-day13.dts
"$DTC_BIN" -I dts -O dtb -o virt-day13.dtb virt-day13.dts

"$QEMU_BIN"     -machine virt     -cpu cortex-a57     -m 1024     -nographic     -kernel "$KERNEL_IMG"     -dtb virt-day13.dtb     -initrd rootfs.img     -append "console=ttyAMA0 root=/dev/ram0 rw rdinit=/init"
