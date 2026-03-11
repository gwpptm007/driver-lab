# 文件作用说明（初学版）

这一页专门解释仓库里最容易搞混的文件分别是干什么的。

---

## 1. demo.c

驱动源码。

你在这里实现：

- 模块加载 / 卸载
- open / release / read / write / ioctl 等文件操作
- sysfs 属性
- debugfs 调试接口

可以把它理解成：

**“真正的驱动逻辑都在这里。”**

---

## 2. Makefile

模块编译规则。

它告诉内核构建系统：

- 要编译哪个 `.c`
- 生成哪个 `.ko`
- 编译时使用哪个内核源码目录

例如：

```make
obj-m += demo.o
KDIR ?= ../../kernel-src/linux-5.15.10/build/x86   # 示例，实际以 build.sh/环境变量为准
```

可以把它理解成：

**“怎么把 demo.c 编译成 demo.ko。”**

现在仓库里的 `build.sh` 还会显式把 `KDIR` 传给 Makefile，这样相对路径、环境变量和旧路径兼容规则可以统一到一处。

---

## 3. build.sh

一键实验脚本。

它通常负责：

1. 编译驱动
2. 编译用户态测试程序
3. 准备 rootfs 目录
4. 拷贝 busybox
5. 生成 `/init`
6. 打包 `rootfs.img`
7. 启动 QEMU

可以把它理解成：

**“从源码到启动实验环境的总控脚本。”**

---

## 4. busybox 是干什么的

Busybox 是最小 Linux 用户态工具集合。

在最小 rootfs 里，我们通常没有完整的 Ubuntu / Debian 用户空间，
所以需要一个很小但功能足够的工具包，提供：

- `/bin/sh`
- `ls`
- `cat`
- `echo`
- `mount`
- `insmod`
- `dmesg`

这就是 busybox 的作用。

你可以把它理解成：

**“最小 Linux 系统的基础命令工具箱。”**

---

## 5. rootfs 是什么

rootfs = 根文件系统。

Linux 内核启动后，不只是需要内核自己，还要有一个用户空间根目录，里面至少要能找到：

- `/init`
- `/bin/sh`
- `/dev`
- `/proc`
- `/sys`

我们在仓库里临时创建的 `rootfs/` 目录，就是这个根文件系统的原型目录。

---

## 6. /init 是干什么的

`/init` 是内核启动后尝试执行的第一个用户态程序。

在最小实验环境里，它通常是一个 shell 脚本，负责：

- 挂载 `/proc`
- 挂载 `/sys`
- 挂载 `/dev`
- 挂载 `debugfs`（如果需要）
- `insmod demo.ko`
- 最后 `exec /bin/sh`

可以把它理解成：

**“最小 Linux 系统启动后的第一段初始化脚本。”**

---

## 7. rootfs.img 是什么

`rootfs.img` 是把 `rootfs/` 打包后的镜像。

通常通过：

```bash
find . | cpio -o -H newc | gzip -9 > ../rootfs.img
```

生成。

QEMU 启动时通过：

```bash
-initrd rootfs.img
```

把它交给内核使用。

---

## 8. QEMU 在这里干什么

QEMU 提供一个轻量实验机。

它的作用是：

- 不污染宿主机系统
- 可以快速重复启动测试
- 便于调试你自己的内核和模块

在这个仓库里，QEMU 启动时通常加载：

- 内核：`../kernel-src/linux-5.15.10/build/x86/arch/x86/boot/bzImage` 或兼容旧路径
- initramfs：`rootfs.img`

---

## 9. 一次完整实验链路

```text
demo.c
  -> Makefile 编译成 demo.ko
  -> build.sh 准备 rootfs/
  -> 拷贝 busybox 和 demo.ko
  -> 生成 /init
  -> 打包 rootfs.img
  -> QEMU 启动 kernel + rootfs.img
  -> /init 执行 insmod /demo.ko
  -> 进入 /bin/sh
  -> 你开始验证 /dev /sys /debugfs
```

如果你以后忘了每个文件干什么，就先回来读这一页。


## QEMU 退出说明

当前实验默认使用 `-nographic` + `console=ttyS0`，所以 QEMU 会占用当前终端前台运行。这不是卡死，而是 QEMU 正在运行。

常用退出方式：

- 直接退出：`Ctrl+a`，然后按 `x`
- 进入 QEMU monitor：`Ctrl+a`，然后按 `c`，输入 `quit` 回车

不建议每次都用 `kill -9` 杀掉 `build.sh`，除非 QEMU 确实无响应。



## day10

- `day10/demo_irqcnt.c`：Day10 中断计数驱动
- `day10/demo_day10.fragment.dtsi`：Day10 测试 DT 片段
- `day10/build.sh`：Day10 arm64 QEMU 启动脚本
