# busybox-1.36.1 环境准备

本目录应放置 BusyBox 1.36.1 源码树。

GitHub 中只保留本说明文件和占位文件，不提交完整源码。请使用者自行下载并解压到当前目录。

---

## 1. 下载源码

```bash
cd driver-lab/kernel-src
wget https://busybox.net/downloads/busybox-1.36.1.tar.bz2
tar -xf busybox-1.36.1.tar.bz2
```

如果解压后目录名不一致，请整理成：

```text
driver-lab/kernel-src/busybox-1.36.1/
```

---

## 2. 生成默认配置

```bash
cd driver-lab/kernel-src/busybox-1.36.1
make defconfig
```

---

## 3. 编译 BusyBox

```bash
make -j$(nproc)
```

编译完成后，通常会生成：

```text
busybox
```

如果你后面想更稳地用于最小 rootfs，也可以进一步了解 BusyBox 是否为静态链接版本。当前实验脚本会优先从你编译出来的 BusyBox 中挑选可执行文件，而不是直接使用 Ubuntu 宿主机自己的 `/usr/bin/busybox`。原因是宿主机版本常常带有额外依赖，放进极简 rootfs 后不一定能直接运行。

---

## 4. 可选：执行 make install

为了让目录结构更接近最小 rootfs 的使用方式，也可以继续执行：

```bash
make install
```

执行后一般会生成：

```text
_install/bin/busybox
```

---

## 5. 为什么实验要依赖 BusyBox

这个实验不是直接在宿主机运行驱动，而是：

- 用 QEMU 启动一个最小 Linux
- 用 initramfs 作为根文件系统
- 在 guest 内执行 `insmod`、`rmmod`、`sh`、`ls`、`dmesg` 等命令

BusyBox 提供了这些最小命令集合。

在 `build.sh` 中会优先尝试：

1. `_install/bin/busybox`
2. 源码目录下直接生成的 `busybox`

---

## 6. 验证方法

确认下面任意一个路径存在且可执行：

```text
busybox-1.36.1/busybox
busybox-1.36.1/_install/bin/busybox
```

然后进入任意一个 day 目录执行：

```bash
chmod +x build.sh
./build.sh
```

如果脚本能正确输出：

```text
Using busybox: .../busybox-1.36.1/...
```

并成功进入 guest shell，说明 BusyBox 准备完成。
