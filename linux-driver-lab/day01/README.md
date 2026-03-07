# Day01：字符设备最小骨架（miscdevice 入门）

## 1. 学习目标

今天的目标不是做复杂功能，而是先把 **驱动最小闭环** 跑通：

- 能把模块编译成 `demo.ko`
- 能在最小 rootfs 里 `insmod /demo.ko`
- 能自动得到 `/dev/demo`
- 能从用户态触发 `open / write / release`
- 能看懂“用户态 -> VFS -> file_operations -> 驱动回调”这条主链路

这一天对应整个项目的地基。后面的 `ioctl / sysfs / debugfs / waitqueue` 都是在这个骨架上不断往上叠。

---

## 2. 今天的任务清单

- [ ] 看懂 `miscdevice` 是怎么自动创建设备节点的
- [ ] 看懂 `struct file_operations` 的几个基础回调
- [ ] 成功执行 `build.sh`
- [ ] 在 QEMU 最小系统里看到 `/dev/demo`
- [ ] 手动触发一次写操作，让内核打印日志
- [ ] 退出 QEMU 前确认没有 Oops / panic

---

## 3. 相关理论与原理

### 3.1 什么是字符设备

字符设备可以把驱动能力暴露成一个文件节点，例如：

```text
/dev/demo
```

用户态通过：

- `open()`
- `read()`
- `write()`
- `ioctl()`
- `close()`

来访问驱动。

字符设备最重要的接口就是：

```c
struct file_operations
```

这张表告诉 VFS：

- 当用户执行 `open` 时调哪个函数
- 当用户执行 `write` 时调哪个函数
- 当用户执行 `close` 时调哪个函数

### 3.2 为什么 Day01 先用 miscdevice

字符设备有很多注册方式。最完整的一套是：

- `alloc_chrdev_region`
- `cdev_init`
- `cdev_add`
- `class_create`
- `device_create`

但对初学阶段来说太重了。

`miscdevice` 是 Linux 提供的一种“轻量字符设备包装”，它帮你省掉一部分重复流程。优点是：

- 代码短
- 上手快
- 容易先把 `/dev/demo` 跑出来

这也是为什么 Day01 的重点是“通路打通”，不是“框架最完整”。

### 3.3 从用户态到驱动，调用链到底怎么走

今天必须形成这个心智模型：

```text
用户态程序
  -> libc/syscall
  -> VFS
  -> struct file_operations
  -> demo_open / demo_write / demo_release
```

用户态看到的是“文件”；
内核态真正执行的是你在 `demo.c` 里注册的回调函数。

---

## 4. 关键文件说明

- `demo.c`：最小字符设备驱动，使用 `miscdevice`
- `Makefile`：把 `demo.c` 编译成 `demo.ko`
- `build.sh`：一键完成“编译 + 准备 rootfs + 启动 QEMU”

补充理解：

- `build.sh` 会从固定路径拿内核镜像和 busybox
- `build.sh` 会自动创建 `/init`
- `/init` 会挂载 `proc/sys/dev`，然后 `insmod /demo.ko`

---

## 5. 执行步骤

### 5.1 进入目录

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day01
```

### 5.2 给脚本增加执行权限

```bash
chmod +x build.sh
```

### 5.3 运行实验

```bash
./build.sh
```

执行成功后，会进入 QEMU 的最小 shell。

---

## 6. 功能验证步骤

### 6.1 验证模块是否已加载

```sh
ls /dev
```

预期：应该能看到 `demo`。

### 6.2 验证设备节点是否存在

```sh
ls -l /dev/demo
```

预期：

- 设备存在
- 类型通常显示为字符设备 `c`

### 6.3 触发一次 write 回调

BusyBox shell 里可执行：

```sh
echo hello > /dev/demo
```

预期：

- 命令本身不报错
- `dmesg | tail` 能看到类似：

```text
Demo: Device opened
Demo: Received N bytes of data
Demo: Device closed
```

### 6.4 再看一眼最近日志

```sh
dmesg | tail -n 20
```

重点观察：

- 模块是否成功加载
- `open/write/release` 是否都触发了
- 有没有 Oops、warning、panic

---

## 7. 你应该掌握的知识点

做完 Day01 后，至少要能回答下面这些问题：

### 问题 1：为什么 `/dev/demo` 会自动出现？
因为 `misc_register()` 会帮我们向 misc 子系统注册设备，内核配合 devtmpfs/udev 机制可以创建设备节点。

### 问题 2：为什么用户态是“写文件”，驱动里却是 `demo_write()`？
因为 VFS 会把文件操作分发到 `struct file_operations` 里对应的回调。

### 问题 3：Day01 为什么没有真正处理用户缓冲区？
因为今天的目标只是打通调用链，不是做完整的数据交换。真正的用户态/内核态数据搬运在 Day02 的 `ioctl + copy_to_user/copy_from_user` 里展开。

---

## 8. 测试记录

### 本次验证状态

- [ ] `build.sh` 编译通过
- [ ] QEMU 启动成功
- [ ] `/dev/demo` 出现
- [ ] `echo hello > /dev/demo` 成功
- [ ] `dmesg` 能看到 `open/write/release`
- [ ] 无 panic / Oops

### 建议保留的记录

每次做实验，建议把下面内容保存到自己的学习日志里：

```text
日期：
内核版本：5.15.10
实验目录：day01
验证命令：
验证结果：
遇到的问题：
修复方法：
```

---

## 9. 今天最重要的一句话

Day01 学的不是“一个能打印日志的 demo”，而是：

```text
驱动如何把自己挂到 Linux 文件模型上。
```

这个理解一旦建立，后面你再看 `ioctl / sysfs / debugfs / mmap`，都会自然很多。


## QEMU 退出说明

当前实验默认使用 `-nographic` + `console=ttyS0`，所以 QEMU 会占用当前终端前台运行。这不是卡死，而是 QEMU 正在运行。

常用退出方式：

- 直接退出：`Ctrl+a`，然后按 `x`
- 进入 QEMU monitor：`Ctrl+a`，然后按 `c`，输入 `quit` 回车

不建议每次都用 `kill -9` 杀掉 `build.sh`，除非 QEMU 确实无响应。

