# kernel-src 环境准备说明

本目录只负责实验环境本身的准备，不讨论任何具体 day 的学习内容。

环境目标只有两类：

- **x86 环境**
- **arm64 环境**

两类环境共用同一份仓库目录约定，但各自拥有独立的构建目录和输出目录。

---

## 1. 目录约定

```text
kernel-src/
├── README.md
├── linux-5.15.10.tar.xz
├── busybox-1.36.1.tar.bz2
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

### 目录含义

#### `linux-5.15.10.tar.xz`
Linux 内核源码压缩包，放在 `kernel-src/` 根目录。

#### `busybox-1.36.1.tar.bz2`
BusyBox 源码压缩包，放在 `kernel-src/` 根目录。

#### `linux-5.15.10/src/`
Linux 5.15.10 的**纯源码目录**。

建议把压缩包内容解压到这里，而不是直接在 `kernel-src/` 下保留官方解压出来的同名目录。这样可以避免：

- `linux-5.15.10/linux-5.15.10/` 这种套娃目录
- 在源码根目录里混入构建输出和最终产物

#### `linux-5.15.10/build/x86/`
Linux 5.15.10 的 x86 构建目录。

这里会生成：

- `.config`
- `Module.symvers`
- `vmlinux`
- 各类中间目标文件
- `arch/x86/boot/bzImage`

外部模块编译时，通常把这里作为 `KDIR` 使用。

#### `linux-5.15.10/build/arm64/`
Linux 5.15.10 的 arm64 构建目录。

这里会生成：

- `.config`
- `Module.symvers`
- `vmlinux`
- `arch/arm64/boot/Image`
- 设备树相关产物

arm64 外部模块编译时，通常把这里作为 `KDIR` 使用。

#### `linux-5.15.10/output/x86/`
Linux 5.15.10 的 x86 最终产物目录。

建议至少放：

- `bzImage`

#### `linux-5.15.10/output/arm64/`
Linux 5.15.10 的 arm64 最终产物目录。

建议至少放：

- `Image`
- 需要的 `dtb`

#### `busybox-1.36.1/src/`
BusyBox 1.36.1 的**纯源码目录**。

#### `busybox-1.36.1/build/x86/`
BusyBox 的 x86 构建目录。

#### `busybox-1.36.1/build/arm64/`
BusyBox 的 arm64 构建目录。

#### `busybox-1.36.1/output/x86/`
BusyBox 的 x86 最终产物目录。

建议至少放：

- `busybox`
- `_install/`

#### `busybox-1.36.1/output/arm64/`
BusyBox 的 arm64 最终产物目录。

建议至少放：

- `busybox`
- `_install/`

---

## 2. 为什么有些实验依赖 BusyBox

很多驱动实验不是只编一个 `.ko` 文件，而是要顺手准备一个最小 rootfs，再用 QEMU 启动测试环境。

在这种场景里，BusyBox 的作用是提供最小用户空间，包括：

- `/bin/sh`
- `mount`
- `insmod`
- `rmmod`
- `dmesg`
- `ls`
- `cat`
- `mkdir`
- 其他常用命令

也就是说，BusyBox 不是“可有可无的附加项”，而是最小实验环境的基础组件之一。

如果某个实验需要：

- 打包 initramfs
- 进入最小 shell
- 在 guest 内加载模块
- 观察 `/proc`、`/sys`、`dmesg`

那它通常就会依赖 BusyBox 提供最小命令集。

---

## 3. Ubuntu 宿主机依赖安装

建议在 Ubuntu 上先安装下面这些基础依赖：

```bash
sudo apt update
sudo apt install -y \
    build-essential bc bison flex libssl-dev libelf-dev libncurses-dev \
    cpio xz-utils bzip2 wget curl git file rsync \
    qemu-system-x86 qemu-system-arm qemu-utils \
    gcc-aarch64-linux-gnu device-tree-compiler
```

### 依赖说明

- `build-essential`：gcc、make 等基础构建工具
- `bc`：内核构建时常用
- `bison`、`flex`：内核构建依赖
- `libssl-dev`、`libelf-dev`、`libncurses-dev`：内核配置与构建依赖
- `cpio`：打包 initramfs 时使用
- `xz-utils`、`bzip2`：解压源码包
- `wget`、`curl`：下载源码
- `file`：检查生成的二进制架构
- `rsync`：拷贝产物时常用
- `qemu-system-x86`：x86 QEMU
- `qemu-system-arm`：arm/arm64 QEMU，通常包含 `qemu-system-aarch64`
- `qemu-utils`：镜像相关工具
- `gcc-aarch64-linux-gnu`：arm64 交叉编译器
- `device-tree-compiler`：处理 `dts/dtb`

安装完成后，建议确认下面两个命令可用：

```bash
qemu-system-x86_64 --version
aarch64-linux-gnu-gcc --version
```

如果需要 arm64 QEMU，再确认：

```bash
qemu-system-aarch64 --version
```

---

## 4. 源码下载方式

### Linux 5.15.10

可以直接在 `kernel-src/` 根目录下载：

```bash
cd kernel-src
wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.10.tar.xz
```

### BusyBox 1.36.1

```bash
cd kernel-src
wget https://busybox.net/downloads/busybox-1.36.1.tar.bz2
```

如果你已经手动下载好了压缩包，也可以直接把它们放到 `kernel-src/` 根目录。

---

## 5. 源码安装与解压

建议使用 `--strip-components=1` 把源码直接解压到 `src/` 目录，避免多一层同名目录。

### 解压 Linux 5.15.10

```bash
cd kernel-src
mkdir -p linux-5.15.10/src
rm -rf linux-5.15.10/src/*
tar -xf linux-5.15.10.tar.xz --strip-components=1 -C linux-5.15.10/src
```

### 解压 BusyBox 1.36.1

```bash
cd kernel-src
mkdir -p busybox-1.36.1/src
rm -rf busybox-1.36.1/src/*
tar -xf busybox-1.36.1.tar.bz2 --strip-components=1 -C busybox-1.36.1/src
```

解压完成后，应至少能看到：

```text
kernel-src/linux-5.15.10/src/Makefile
kernel-src/busybox-1.36.1/src/Makefile
```

---

## 6. x86 环境编译

### 6.1 编译 x86 Linux 内核

```bash
cd kernel-src/linux-5.15.10/src
make O=../build/x86 x86_64_defconfig
make O=../build/x86 -j$(nproc)
```

常用产物：

```text
kernel-src/linux-5.15.10/build/x86/arch/x86/boot/bzImage
kernel-src/linux-5.15.10/build/x86/Module.symvers
```

建议把 `bzImage` 拷贝到输出目录：

```bash
cp ../build/x86/arch/x86/boot/bzImage ../output/x86/
```

### 6.2 编译 x86 BusyBox

很多最小 rootfs / initramfs 实验要求 BusyBox 使用**静态链接**。
如果 BusyBox 是动态链接版，拷进 rootfs 后常会因为缺少 `/lib64/ld-linux-x86-64.so.2` 和 glibc 依赖而导致 `/init`、`/bin/sh` 无法执行。

建议按下面步骤编译：

```bash
cd kernel-src/busybox-1.36.1/src
make O=../build/x86 defconfig
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' ../build/x86/.config
make O=../build/x86 oldconfig
make O=../build/x86 -j$(nproc)
cp ../build/x86/busybox ../output/x86/
make O=../build/x86 CONFIG_PREFIX=$(pwd)/../output/x86/_install install
```

编译完成后建议立刻检查：

```bash
file kernel-src/busybox-1.36.1/output/x86/_install/bin/busybox
```

期望输出中包含：

```text
statically linked
```

常用产物：

```text
kernel-src/busybox-1.36.1/output/x86/busybox
kernel-src/busybox-1.36.1/output/x86/_install/bin/busybox
```

---

## 7. arm64 环境编译

### 7.1 编译 arm64 Linux 内核

```bash
cd kernel-src/linux-5.15.10/src
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs -j$(nproc)
```

常用产物：

```text
kernel-src/linux-5.15.10/build/arm64/arch/arm64/boot/Image
kernel-src/linux-5.15.10/build/arm64/Module.symvers
kernel-src/linux-5.15.10/build/arm64/arch/arm64/boot/dts/
```

建议把常用产物拷贝到输出目录：

```bash
cp ../build/arm64/arch/arm64/boot/Image ../output/arm64/
```

如果后面需要某个 dtb，也可按需拷贝：

```bash
cp ../build/arm64/arch/arm64/boot/dts/<your-board>.dtb ../output/arm64/
```

### 7.2 编译 arm64 BusyBox

arm64 最小 rootfs 同样建议使用静态链接 BusyBox。

```bash
cd kernel-src/busybox-1.36.1/src
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' ../build/arm64/.config
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- oldconfig
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
cp ../build/arm64/busybox ../output/arm64/
make O=../build/arm64 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CONFIG_PREFIX=$(pwd)/../output/arm64/_install install
```

编译完成后建议检查：

```bash
file kernel-src/busybox-1.36.1/output/arm64/_install/bin/busybox
```

期望输出中包含：

```text
statically linked
```

常用产物：

```text
kernel-src/busybox-1.36.1/output/arm64/busybox
kernel-src/busybox-1.36.1/output/arm64/_install/bin/busybox
```

---

## 8. 外部模块和实验脚本应该使用哪些路径

### x86 环境常用路径

#### 内核构建目录

```text
kernel-src/linux-5.15.10/build/x86
```

这个目录通常作为外部模块编译时的 `KDIR`。

#### 内核镜像

```text
kernel-src/linux-5.15.10/output/x86/bzImage
```

#### BusyBox 安装目录

```text
kernel-src/busybox-1.36.1/output/x86/_install
```

#### BusyBox 可执行文件

```text
kernel-src/busybox-1.36.1/output/x86/_install/bin/busybox
```

### arm64 环境常用路径

#### 内核构建目录

```text
kernel-src/linux-5.15.10/build/arm64
```

#### 内核镜像

```text
kernel-src/linux-5.15.10/output/arm64/Image
```

#### BusyBox 安装目录

```text
kernel-src/busybox-1.36.1/output/arm64/_install
```

#### BusyBox 可执行文件

```text
kernel-src/busybox-1.36.1/output/arm64/_install/bin/busybox
```

---

## 9. 最小检查项

### x86 检查

```bash
file kernel-src/linux-5.15.10/output/x86/bzImage
file kernel-src/busybox-1.36.1/output/x86/_install/bin/busybox
```

期望 BusyBox 输出同时体现：

- `x86-64`
- `statically linked`

### arm64 检查

```bash
file kernel-src/linux-5.15.10/output/arm64/Image
file kernel-src/busybox-1.36.1/output/arm64/_install/bin/busybox
```

期望 arm64 BusyBox 输出同时体现：

- `ARM aarch64`
- `statically linked`

---

## 10. 使用建议

- 压缩包放在 `kernel-src/` 根目录
- 解压内容进入各版本目录下的 `src/`
- 构建输出始终放 `build/`
- 最终给实验直接使用的文件放 `output/`
- 外部模块编译优先使用 `build/<arch>` 作为 `KDIR`
- QEMU 启动优先使用 `output/<arch>` 下的镜像
