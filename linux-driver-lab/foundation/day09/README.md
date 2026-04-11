# Day09 - 第一次在 x86 Ubuntu 上跑 ARM64 QEMU Device Tree 实验

> 本 README 只关心一件事：
>
> **如何在当前项目结构下，把 arm64 环境准备好，然后把 Day09 跑起来。**

---

## 1. 先说清楚宿主机和目标机

你当前的开发环境通常是：

- Windows
- VMware
- VMware 里的 Ubuntu x86_64

Day09 不是让你把 Ubuntu 重装成 arm64，也不是要求你买 ARM 开发板。

Day09 的方式是：

- **宿主机**：仍然是 x86 Ubuntu
- **目标机**：通过 `qemu-system-aarch64` 模拟一台 arm64 `virt` 机器

也就是说，当前实验链路是：

```text
x86 Ubuntu 宿主机
    -> aarch64-linux-gnu- 交叉编译 arm64 内核模块
    -> qemu-system-aarch64 启动 arm64 虚拟机
    -> 在 arm64 guest 中加载 demo_of.ko
```

所以 Day09 的关键不是换电脑，而是：

**在当前 x86 Ubuntu 上准备出一套可用的 arm64 环境。**

---

## 2. 当前项目里的环境目录约定

当前仓库已经约定好 `kernel-src` 结构：

```text
kernel-src/
├── linux-5.15.10/
│   ├── src/
│   ├── build/
│   │   ├── x86/
│   │   └── arm64/
│   └── output/
│       ├── x86/
│       └── arm64/
└── busybox-1.36.1/
    ├── src/
    ├── build/
    │   ├── x86/
    │   └── arm64/
    └── output/
        ├── x86/
        └── arm64/
```

Day09 只关心 arm64 这几个位置。

### arm64 内核源码
```text
kernel-src/linux-5.15.10/src
```

### arm64 内核构建目录
```text
kernel-src/linux-5.15.10/build/arm64
```

### arm64 内核镜像输出目录
```text
kernel-src/linux-5.15.10/output/arm64
```

### arm64 BusyBox 源码
```text
kernel-src/busybox-1.36.1/src
```

### arm64 BusyBox 构建目录
```text
kernel-src/busybox-1.36.1/build/arm64
```

### arm64 BusyBox 安装输出目录
```text
kernel-src/busybox-1.36.1/output/arm64/_install
```

---

## 3. 宿主机依赖安装

先在 Ubuntu 宿主机安装这些工具：

```bash
sudo apt update
sudo apt install -y \
    build-essential bc bison flex libssl-dev libelf-dev libncurses-dev \
    cpio xz-utils bzip2 file wget curl git rsync \
    qemu-system-arm qemu-utils \
    gcc-aarch64-linux-gnu device-tree-compiler
```

安装完成后，确认这几个命令存在：

```bash
qemu-system-aarch64 --version
aarch64-linux-gnu-gcc --version
dtc --version
```

---

## 4. 先准备 arm64 Linux 内核

如果 `kernel-src/linux-5.15.10/src` 还没有源码，请先解压到这里。

进入内核源码目录：

```bash
cd /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src
```

### 4.1 生成 arm64 配置

```bash
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
```

### 4.2 编译 arm64 内核

```bash
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
```

### 4.3 拷贝 Image 到 output 目录

```bash
mkdir -p ../output/arm64
cp ../build/arm64/arch/arm64/boot/Image ../output/arm64/Image
```

### 4.4 确认产物存在

```bash
file ../output/arm64/Image
```

看到 `ARM aarch64` 相关信息就说明方向对了。

---

## 5. 再准备 arm64 BusyBox

进入 BusyBox 源码目录：

```bash
cd /home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1/src
```

### 5.1 生成 arm64 配置

```bash
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
```

### 5.2 打开静态链接选项

```bash
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- menuconfig
```

进入菜单后打开：

```text
Busybox Settings  --->
    [*] Build static binary (no shared libs)
```

也就是确保：

```text
CONFIG_STATIC=y
```

### 5.3 编译 arm64 BusyBox

```bash
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
```

### 5.4 安装到 output 目录

```bash
make O=../build/arm64 \
    ARCH=arm64 \
    CROSS_COMPILE=aarch64-linux-gnu- \
    CONFIG_PREFIX=$(pwd)/../output/arm64/_install install
```

### 5.5 检查 BusyBox 是否为静态链接

```bash
file ../output/arm64/_install/bin/busybox
```

预期要看到类似：

```text
statically linked
```

如果看到的是 `dynamically linked`，那最小 rootfs 很容易起不来。

---

## 6. Day09 最推荐的执行方式

进入 Day09 目录：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day09
```

然后按下面方式准备变量：

```bash
export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-
./build.sh
```

这是当前项目里**最推荐**的用法。

为什么这样更好：

- `KERNEL_DIR` 指向某个版本内核的工作区根目录
- `BUSYBOX_DIR` 指向某个版本 BusyBox 的工作区根目录
- `build.sh` 再从里面自动找到：
  - `build/arm64`
  - `output/arm64/Image`
  - `output/arm64/_install`

这样以后如果你要换版本，比如从 `linux-5.15.10` 换到别的版本，只要改 `KERNEL_DIR` 就行。

---

## 7. `build.sh` 实际会做什么

`./build.sh` 会顺序完成下面这些动作：

### 7.1 编译 arm64 外部模块
生成：

```text
demo_of.ko
```

### 7.2 组最小 arm64 rootfs
把静态链接 BusyBox 和 `demo_of.ko` 打包进 initramfs。

### 7.3 让 QEMU 导出基础 DTB
生成：

```text
virt-base.dtb
virt-base.dts
```

### 7.4 把 Day09 的 DT 片段注入进去
生成：

```text
virt-day09.dts
virt-day09.dtb
```

### 7.5 启动 arm64 QEMU 虚拟机
最后进入 guest，等你手工执行：

```bash
insmod /demo_of.ko
dmesg | grep demo_of
```

---

## 8. `build.sh` 关键流程详细解释

这一节专门解释脚本里比较容易看不懂的几段。

### 8.1 让 `/init` 可执行

```bash
chmod +x "$ROOTFS/init"
```

这里是给 rootfs 里的 `init` 脚本加执行权限。

后面 QEMU 启动时，会通过：

```bash
-append "console=ttyAMA0 root=/dev/ram0 rw rdinit=/init"
```

告诉内核：

> 从 initramfs 里执行 `/init` 作为第一个用户态进程

如果没有执行权限，内核会报：

- `Failed to execute /init`
- 然后可能继续找 `/bin/sh`
- 最后 panic

---

### 8.2 把 rootfs 目录打包成 initramfs 镜像

```bash
(
    cd "$ROOTFS"
    find . | cpio -o -H newc | gzip -9 > ../rootfs.img
)
```

这段的作用是把当前 `rootfs/` 目录打成 `rootfs.img`。

分解理解：

- `cd "$ROOTFS"`  
  先进入 rootfs 目录，确保打包进去的是相对路径，而不是宿主机绝对路径

- `find .`  
  把 rootfs 下面所有文件列出来

- `cpio -o -H newc`  
  按 `newc` 格式生成 initramfs，Linux 内核对这个格式支持很好

- `gzip -9`  
  压缩成 gzip 格式

- `> ../rootfs.img`  
  输出到 day09 目录下的 `rootfs.img`

最终产物就是：

```text
day09/rootfs.img
```

这个文件后面由 QEMU 的 `-initrd` 使用。

---

### 8.3 让 QEMU 导出基础 DTB

```bash
"$QEMU_BIN" \
    -machine virt,dumpdtb=virt-base.dtb \
    -cpu cortex-a57 \
    -m 512 \
    -nographic \
    >/dev/null 2>&1 || true
```

这一段不是在真正启动系统，而是在做一件事：

> 让 QEMU 把 `virt` 这台 ARM64 机器默认的设备树导出来

重点参数是：

- `-machine virt,dumpdtb=virt-base.dtb`  
  表示导出 DTB 到 `virt-base.dtb`

- `-cpu cortex-a57`  
  指定 CPU 型号

- `-m 512`  
  这里内存给 512 就够了，因为只是为了导出一份基础 DTB

- `-nographic`  
  走终端模式，不起图形界面

- `>/dev/null 2>&1`  
  丢弃输出，因为这里只关心文件生成，不关心启动日志

- `|| true`  
  有些情况下 QEMU 导出完 DTB 以后退出码不一定完美，为了避免脚本因为 `set -e` 提前退出，这里先放过，后面再靠文件存在性检查来兜底

紧接着脚本会做检查：

```bash
if [ ! -f virt-base.dtb ]; then
    echo "[ERROR] failed to dump QEMU virt base DTB"
    exit 1
fi
```

也就是说：

- 不完全相信退出码
- 最终以 `virt-base.dtb` 是否真的生成成功为准

---

### 8.4 把基础 DTB 反编译成 DTS

```bash
"$DTC_BIN" -I dtb -O dts -o virt-base.dts virt-base.dtb
```

作用是：

> 把二进制设备树 `virt-base.dtb` 反编译成可读、可编辑的文本版 `virt-base.dts`

参数解释：

- `-I dtb`：输入格式是 dtb
- `-O dts`：输出格式是 dts
- `-o virt-base.dts`：输出文件名

为什么要这么做：

- `dtb` 是给机器用的
- `dts` 是给人和脚本改的

我们后面要把 Day09 自己的测试节点插进去，所以先要转成文本版。

---

### 8.5 注入 Day09 的设备树片段

```bash
"$PYTHON_BIN" ./inject_virt_dt.py \
    --input virt-base.dts \
    --fragment demo_day09.fragment.dtsi \
    --output virt-day09.dts
```

这一段的作用是：

> 把你自己写的 Day09 测试节点片段，插入到 QEMU 导出的基础设备树里

三个文件的角色分别是：

- `virt-base.dts`  
  QEMU 原始 `virt` 机器的 DTS

- `demo_day09.fragment.dtsi`  
  Day09 自己定义的测试节点片段

- `virt-day09.dts`  
  合成后的完整 DTS

这里使用的是命名参数：

- `--input`
- `--fragment`
- `--output`

不要把它当成普通位置参数来调用。

---

### 8.6 再把 DTS 编译回最终 DTB

```bash
"$DTC_BIN" -I dts -O dtb -o virt-day09.dtb virt-day09.dts
```

作用是：

> 把刚刚合成好的文本版设备树，再编回二进制 DTB

最终得到：

```text
virt-day09.dtb
```

这就是后面真正交给 QEMU 启动使用的设备树。

---

### 8.7 用内核、initramfs、最终 DTB 启动 QEMU

```bash
"$QEMU_BIN" \
    -machine virt \
    -cpu cortex-a57 \
    -m 1024 \
    -nographic \
    -kernel "$KERNEL_IMG" \
    -dtb virt-day09.dtb \
    -initrd rootfs.img \
    -append "console=ttyAMA0 root=/dev/ram0 rw rdinit=/init"
```

这是脚本最后真正启动虚拟机的部分。

参数解释：

- `-machine virt`  
  使用 ARM64 的 `virt` 机器模型

- `-cpu cortex-a57`  
  指定 CPU 类型

- `-m 1024`  
  给虚拟机 1GB 内存

- `-nographic`  
  不使用图形界面，串口输出直接显示在当前终端

- `-kernel "$KERNEL_IMG"`  
  指定 ARM64 内核镜像

- `-dtb virt-day09.dtb`  
  指定刚刚生成的、包含 Day09 测试节点的设备树

- `-initrd rootfs.img`  
  指定刚刚打包好的 initramfs

- `-append "console=ttyAMA0 root=/dev/ram0 rw rdinit=/init"`  
  给内核传参数，其中：
  - `console=ttyAMA0`：控制台走 ARM 串口
  - `root=/dev/ram0`：根文件系统来自 RAM/initramfs
  - `rw`：根文件系统读写挂载
  - `rdinit=/init`：启动时执行 initramfs 里的 `/init`

---

### 8.8 把整条链压成一句话

`build.sh` 的本质就是：

1. 打包 rootfs 得到 `rootfs.img`
2. 从 QEMU 导出基础 DTB
3. 反编译成 DTS
4. 把 Day09 节点片段注入进去
5. 再编译成新的 DTB
6. 用 `Image + rootfs.img + virt-day09.dtb` 启动 QEMU

---

## 9. 第一次跑成功时，你应该看到什么

进入 guest 后，先执行：

```bash
insmod /demo_of.ko
dmesg | grep demo_of
```

成功时，重点看这些日志：

- `probe start`
- `raw DT reg cells`
- `parsed MEM resource`
- `raw DT interrupts cells`
- `parsed Linux IRQ`

```bash
===============================================
 Linux Driver Lab Day09 Device Tree init ready
===============================================

[Guest Tips]
  加载模块           : insmod /demo_of.ko
  卸载模块           : rmmod demo_of
  查看日志           : dmesg | grep demo_of
  查看 DT 节点       : ls /sys/firmware/devicetree/base
  查看平台设备       : ls /sys/bus/platform/devices
  查看平台驱动       : ls /sys/bus/platform/drivers

~ #
~ # insmod /demo_of.ko
[   11.600315] demo_of: loading out-of-tree module taints kernel.
[   11.605187] demo_of_pdrv: module init
[   11.605836] demo_of_pdrv 10000000.demo_dt: probe start
[   11.606258] demo_of_pdrv 10000000.demo_dt: of node full name: demo_dt@10000000
[   11.606647] demo_of_pdrv 10000000.demo_dt: of match data: day09-of-match
[   11.606974] demo_of_pdrv 10000000.demo_dt: dt label: from-qemu-dt
[   11.607503] demo_of_pdrv 10000000.demo_dt: raw DT reg cells: <0x0 0x10000000 0x0 0x1000>
[   11.607967] demo_of_pdrv 10000000.demo_dt: raw DT interrupts cells: <0x0 0x64 0x4>
[   11.608382] demo_of_pdrv 10000000.demo_dt: parsed MEM resource: start=0x10000000 end=0x10000fff size=0x1000
[   11.608859] demo_of_pdrv 10000000.demo_dt: parsed Linux IRQ: 49
~ #
~ # dmesg | grep demo_of
[   11.600315] demo_of: loading out-of-tree module taints kernel.
[   11.605187] demo_of_pdrv: module init
[   11.605836] demo_of_pdrv 10000000.demo_dt: probe start
[   11.606258] demo_of_pdrv 10000000.demo_dt: of node full name: demo_dt@10000000
[   11.606647] demo_of_pdrv 10000000.demo_dt: of match data: day09-of-match
[   11.606974] demo_of_pdrv 10000000.demo_dt: dt label: from-qemu-dt
[   11.607503] demo_of_pdrv 10000000.demo_dt: raw DT reg cells: <0x0 0x10000000 0x0 0x1000>
[   11.607967] demo_of_pdrv 10000000.demo_dt: raw DT interrupts cells: <0x0 0x64 0x4>
[   11.608382] demo_of_pdrv 10000000.demo_dt: parsed MEM resource: start=0x10000000 end=0x10000fff size=0x1000
[   11.608859] demo_of_pdrv 10000000.demo_dt: parsed Linux IRQ: 49
~ #
```

这说明几件事都通了：

1. 设备树节点已经进入系统
2. compatible 已经匹配成功
3. 驱动已经进入 `probe()`
4. `reg/irq` 已经被正确解析出来

---

## 10. 第一次跑 Day09 最常见的失败点

### 10.1 `qemu-system-aarch64: command not found`
说明宿主机没装 QEMU arm64 相关工具：

```bash
sudo apt install qemu-system-arm
```

### 10.2 `aarch64-linux-gnu-gcc: command not found`
说明没装交叉编译器：

```bash
sudo apt install gcc-aarch64-linux-gnu
```

### 10.3 `dtc: command not found`
说明没装设备树编译器：

```bash
sudo apt install device-tree-compiler
```

### 10.4 BusyBox 是动态链接版
如果 `file .../busybox` 看到：

```text
dynamically linked
```

那最小 rootfs 很可能起不来。

必须重新打开：

```text
CONFIG_STATIC=y
```

然后重编。

### 10.5 `arm64 kernel Image not found`
说明内核虽然编了，但还没有把 `Image` 放到：

```text
kernel-src/linux-5.15.10/output/arm64/Image
```

按 README 第 4 节补一次拷贝即可。

---

## 11. 如果你想手工分步执行

如果你不想一步到位跑 `./build.sh`，也可以这样分步做。

### 11.1 先单独编模块

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day09
make KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10 \
     KDIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64 \
     ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
```

### 11.2 再执行完整构建和启动

```bash
export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-
./build.sh
```

---

## 12. 当前目录里的关键文件

### `demo_of.c`
Day09 驱动本体。

### `demo_day09.fragment.dtsi`
要注入到 QEMU `virt` 基础设备树里的测试节点片段。

### `inject_virt_dt.py`
把 Day09 的 DT 片段插入到导出的基础 DTS 中。

### `build.sh`
一键完成模块编译、rootfs 打包、DT 注入、QEMU 启动。

### `prereadme.md`
Day09 的预热说明，先帮助你建立 DT 和 compatible/reg/interrupts 的基本认识。

---

## 13. 推荐执行顺序

第一次跑，建议严格按这个顺序：

1. 安装宿主机依赖
2. 编 arm64 内核
3. 拷贝 `Image` 到 `output/arm64`
4. 编 arm64 静态 BusyBox
5. 检查 BusyBox 是否是 `statically linked`
6. 导出 `KERNEL_DIR / BUSYBOX_DIR / CROSS_COMPILE`
7. 执行 `./build.sh`
8. 在 guest 中 `insmod /demo_of.ko`
9. 用 `dmesg | grep demo_of` 验证

按这个顺序走，最不容易乱。
