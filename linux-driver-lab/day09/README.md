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

Day09 只关心 arm64 这几个位置：

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
sudo apt install -y     build-essential bc bison flex libssl-dev libelf-dev libncurses-dev     cpio xz-utils bzip2 file wget curl git rsync     qemu-system-arm qemu-utils     gcc-aarch64-linux-gnu device-tree-compiler
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
make O=../build/arm64     ARCH=arm64     CROSS_COMPILE=aarch64-linux-gnu-     CONFIG_PREFIX=$(pwd)/../output/arm64/_install install
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

## 8. 第一次跑成功时，你应该看到什么

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

## 9. 第一次跑 Day09 最常见的失败点

### 9.1 `qemu-system-aarch64: command not found`
说明宿主机没装 QEMU arm64 相关工具：

```bash
sudo apt install qemu-system-arm
```

### 9.2 `aarch64-linux-gnu-gcc: command not found`
说明没装交叉编译器：

```bash
sudo apt install gcc-aarch64-linux-gnu
```

### 9.3 `dtc: command not found`
说明没装设备树编译器：

```bash
sudo apt install device-tree-compiler
```

### 9.4 BusyBox 是动态链接版
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

### 9.5 `arm64 kernel Image not found`
说明内核虽然编了，但还没有把 `Image` 放到：

```text
kernel-src/linux-5.15.10/output/arm64/Image
```

按 README 第 4 节补一次拷贝即可。

---

## 10. 如果你想手工分步执行

如果你不想一步到位跑 `./build.sh`，也可以这样分步做：

### 10.1 先单独编模块

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day09
make KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10      KDIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64      ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
```

### 10.2 再执行完整构建和启动

```bash
export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-
./build.sh
```

---

## 11. 当前目录里的关键文件

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

## 12. 推荐执行顺序

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
