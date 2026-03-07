# Day03：sysfs 属性接口、状态控制与测试记录

## 1. 学习目标

Day03 开始把驱动从“只有功能入口”推进到“有控制面、有状态面”的形态。

今天的目标是：

- 在 `/sys/class/demo/demo/` 下导出驱动属性
- 通过 `enable` 动态打开/关闭驱动能力
- 通过 `counter` 统计成功的 ioctl 次数
- 理解 `class_create / device_create / DEVICE_ATTR_*` 的作用
- 学会如何用 `cat/echo` 直接控制驱动行为
- 形成完整测试记录，知道怎么验证、怎么留痕

这是第一次让驱动真正具备“可控制、可观测”的味道。

---

## 2. 今天的任务清单

- [ ] 成功执行 `build.sh`
- [ ] 确认 `/dev/demo` 自动生成
- [ ] 确认 `/sys/class/demo/demo/enable` 存在
- [ ] 确认 `/sys/class/demo/demo/counter` 存在
- [ ] 运行 `/bin/test_ioctl` 验证 SET/GET
- [ ] 关闭 `enable` 后验证 ioctl 被拒绝
- [ ] 再次开启 `enable`，验证功能恢复
- [ ] 保留完整测试记录

---

## 3. 相关理论与原理

### 3.1 什么是 sysfs

`sysfs` 是 Linux 的一个虚拟文件系统，挂载在：

```text
/sys
```

它的作用是把内核里的对象、属性、层次关系导出给用户空间。

在驱动学习里，sysfs 最常见的用途是导出：

- 配置项
- 状态项
- 简单统计
- 调试辅助信息

你可以把它理解成：

```text
给驱动加一个“控制面面板”和“状态面板”。
```

### 3.2 为什么 Day03 要引入 class_create / device_create

Day01/Day02 使用的是 `miscdevice`，更适合快速入门。

Day03 为了学习 sysfs，开始显式引入：

- `class_create()`
- `device_create()`
- `device_create_file()`

它们的作用分别是：

- `class_create()`：创建一个设备类，例如 `demo`
- `device_create()`：在这个类下面创建设备对象，例如 `demo`
- `device_create_file()`：给设备对象挂属性文件

于是就形成了路径：

```text
/sys/class/demo/demo/enable
/sys/class/demo/demo/counter
```

### 3.3 Show / Store 回调是怎么工作的

sysfs 属性本质上是“文件接口映射到驱动回调”。

#### 读属性：show

当用户执行：

```sh
cat /sys/class/demo/demo/enable
```

内核就会调用：

```c
enable_show(...)
```

然后驱动把内核变量格式化成字符串返回。

#### 写属性：store

当用户执行：

```sh
echo 0 > /sys/class/demo/demo/enable
```

内核就会调用：

```c
enable_store(...)
```

驱动把字符串解析成整数，再更新内核变量。

### 3.4 Day03 的控制逻辑

Day03 引入了两个内核变量：

- `demo_enable`：总开关
- `demo_counter`：成功 ioctl 次数统计

驱动在 `demo_ioctl()` 入口先判断：

```c
if (!demo_enable)
    return -EPERM;
```

这就实现了一个很典型的模式：

```text
sysfs 决定策略
ioctl 提供功能
counter 提供状态观察
```

这也是后面真实驱动里很常见的设计思路。

### 3.5 为什么这一天特别要关注 busybox / rootfs / init

你这次已经实测踩过一个很典型的问题：

```text
/init exists but couldn't execute
/bin/sh exists but couldn't execute
```

这不是驱动代码本身的问题，而是最小 rootfs 启动链路出了问题。

Day03 的 `build.sh` 现在已经统一改成优先使用：

```text
/home/wq7/workspace/kernel-src/busybox-1.36.1/_install/bin/busybox
/home/wq7/workspace/kernel-src/busybox-1.36.1/busybox
```

目的就是避免误用宿主机动态链接版 busybox，导致 `/init -> /bin/sh -> busybox` 这条执行链断掉。

---

## 4. 关键文件说明

- `demo.c`：sysfs + ioctl 核心驱动逻辑
- `demo_ioctl.h`：共享 ioctl 协议定义
- `test_ioctl.c`：用户态测试程序
- `Makefile`：模块与静态测试程序构建规则
- `build.sh`：编译、打包 rootfs、启动 QEMU

---

## 5. 执行步骤

### 5.1 进入目录

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day03
```

### 5.2 给脚本增加执行权限

```bash
chmod +x build.sh
```

### 5.3 运行实验

```bash
./build.sh
```

执行完成后会进入 QEMU 最小 shell。

---

## 6. 功能验证步骤

### 6.1 基础环境确认

#### 查看 sysfs 目录和设备节点

```sh
ls -l /sys/class/demo/demo/
ls -l /dev/demo
```

预期：

- `enable` 和 `counter` 存在
- `/dev/demo` 已自动生成

### 6.2 测试 counter 的联动

```sh
cat /sys/class/demo/demo/counter
/bin/test_ioctl
cat /sys/class/demo/demo/counter
```

预期：

- 初始值通常为 `0`
- `test_ioctl` 内部会执行一次 `SET` 和一次 `GET`
- 所以 `counter` 每次应增加 `2`

### 6.3 测试 enable 的拦截能力

```sh
echo 0 > /sys/class/demo/demo/enable
/bin/test_ioctl
```

预期：

- 用户态打印 `Operation not permitted`
- `dmesg | tail` 能看到：

```text
Demo: Device is disabled! Operation rejected.
```

### 6.4 恢复功能并再次验证

```sh
echo 1 > /sys/class/demo/demo/enable
/bin/test_ioctl
cat /sys/class/demo/demo/counter
```

预期：

- SET/GET 再次成功
- 计数继续累加

---

## 7. 实际测试记录（已保留）

下面这段是你已经完成并验证过的实验记录，后续可以继续追加：

```text
/ # cat /sys/class/demo/demo/enable
1

/ # /bin/test_ioctl
--- Day 03 Sysfs & IOCTL 测试 ---
[   34.394822] Demo: Received cmd=0x40046b02, expected GET=0x80046b01, SET=0x40046b02
   SET 成功: 发送了 88
[   34.397295] Demo: Received cmd=0x80046b01, expected GET=0x80046b01, SET=0x40046b02
   GET 成功: 收到了 88
--- 测试结束 ---

/ # ls -l /sys/class/demo/demo/
total 0
-r--r--r--    1 0        0             4096 Mar  5 09:40 counter
-r--r--r--    1 0        0             4096 Mar  5 09:40 dev
-rw-r--r--    1 0        0             4096 Mar  5 09:40 enable
drwxr-xr-x    2 0        0                0 Mar  5 09:40 power
lrwxrwxrwx    1 0        0                0 Mar  5 09:40 subsystem -> ../../../../class/demo
-rw-r--r--    1 0        0             4096 Mar  5 09:40 uevent

/ # ls -l /dev/demo
crw-------    1 0        0         248,   0 Mar  5 09:39 /dev/demo

/ # cat /sys/class/demo/demo/counter
2

/ # /bin/test_ioctl
--- Day 03 Sysfs & IOCTL 测试 ---
[  336.577282] Demo: Received cmd=0x40046b02, expected GET=0x80046b01, SET=0x40046b02
   SET 成功: 发送了 88
[  336.578005] Demo: Received cmd=0x80046b01, expected GET=0x80046b01, SET=0x40046b02
   GET 成功: 收到了 88
--- 测试结束 ---

/ # cat /sys/class/demo/demo/counter
4

/ # echo 0 > /sys/class/demo/demo/enable
[  403.615850] Demo: Enable set to 0

/ # /bin/test_ioctl
--- Day 03 Sysfs & IOCTL 测试 ---
[  409.829547] Demo: Received cmd=0x40046b02, expected GET=0x80046b01, SET=0x40046b02
[  409.830231] Demo: Device is disabled! Operation rejected.
   SET 失败: Operation not permitted
[  409.832025] Demo: Received cmd=0x80046b01, expected GET=0x80046b01, SET=0x40046b02
[  409.832375] Demo: Device is disabled! Operation rejected.
   GET 失败: Operation not permitted
--- 测试结束 ---

/ # echo 1 > /sys/class/demo/demo/enable
[  422.337854] Demo: Enable set to 1

/ # /bin/test_ioctl
--- Day 03 Sysfs & IOCTL 测试 ---
[  424.714275] Demo: Received cmd=0x40046b02, expected GET=0x80046b01, SET=0x40046b02
   SET 成功: 发送了 88
[  424.715245] Demo: Received cmd=0x80046b01, expected GET=0x80046b01, SET=0x40046b02
   GET 成功: 收到了 88
--- 测试结束 ---
```

---

## 8. 从测试记录里应该看懂什么

### 8.1 控制流已经验证通过

`echo 0 > enable` 后，后续 `ioctl` 会被统一拒绝，并返回：

```text
EPERM / Operation not permitted
```

这说明 sysfs 改变了驱动内部行为，而不只是“显示一个变量”。

### 8.2 数据流已经验证通过

`test_ioctl` 能成功完成：

- `SET 88`
- `GET 88`

说明 ioctl 命令编码和 `copy_to_user/copy_from_user` 路径都打通了。

### 8.3 统计流已经验证通过

`counter` 的变化过程：

```text
2 -> 4 -> 6
```

说明每次成功的 `SET + GET` 都能被正确计数。

---

## 9. 你应该掌握的知识点

### 问题 1：为什么 sysfs 更适合做“开关”和“统计”？
因为它天然是属性接口，适合导出简单配置和状态，不需要写专门用户程序就能通过 `cat/echo` 操作。

### 问题 2：`enable` 和 `counter` 分别代表什么角色？
- `enable`：控制面
- `counter`：状态面
- `ioctl`：功能面

### 问题 3：为什么 Day03 是一次重要升级？
因为驱动开始拥有：

- 功能入口（ioctl）
- 控制入口（sysfs）
- 状态观测（counter）

这比只有 `/dev/demo` 的 Day01/Day02 更接近真实驱动工程。

### 问题 4：为什么这一天必须重视 `build.sh`？
因为最小系统如果 `/init` 或 `/bin/sh` 执行失败，根本进不到驱动测试阶段。学驱动不能只盯着 `demo.c`，还要理解最小实验系统的启动链路。

---

## 10. 本次验证结论

### 已验证通过

- [x] `build.sh` 启动成功
- [x] `/dev/demo` 自动生成
- [x] sysfs 属性存在
- [x] `SET/GET` 成功
- [x] `counter` 可统计
- [x] `enable` 能动态拦截 ioctl
- [x] `enable` 恢复后功能重新生效

### 后续可继续完善的点

- 错误路径回滚还不够完整
- `open/release` 目前未认真实现
- `demo_counter` 未加锁，后续进入并发章节时要处理
- 设备注册方式后续建议向 `cdev + 设备对象` 统一收敛

---

## 11. 今天最重要的一句话

Day03 的本质不是“多了两个 sysfs 文件”，而是：

```text
驱动开始同时具备了功能面、控制面和状态面。
```

这一步走通之后，后面的 debugfs、waitqueue、platform、PCIe 都会更顺。


## QEMU 退出说明

当前实验默认使用 `-nographic` + `console=ttyS0`，所以 QEMU 会占用当前终端前台运行。这不是卡死，而是 QEMU 正在运行。

常用退出方式：

- 直接退出：`Ctrl+a`，然后按 `x`
- 进入 QEMU monitor：`Ctrl+a`，然后按 `c`，输入 `quit` 回车

不建议每次都用 `kill -9` 杀掉 `build.sh`，除非 QEMU 确实无响应。

