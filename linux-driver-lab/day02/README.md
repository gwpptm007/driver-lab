# Day02：ioctl 与用户态数据交互

## 1. 学习目标

Day01 打通的是“文件操作链路”，Day02 开始真正进入 **用户态与驱动交换数据**。

今天要完成：

- 理解 ioctl 的作用
- 学会定义共享命令号头文件 `demo_ioctl.h`
- 学会 `copy_from_user / copy_to_user`
- 学会返回标准错误码：`-EINVAL / -EFAULT`
- 用用户态测试程序验证 `SET / GET`

如果 Day01 解决的是“驱动能不能被访问”，那么 Day02 解决的是“驱动和用户程序怎么说话”。

---

## 2. 今天的任务清单

- [ ] 理解 `_IO / _IOR / _IOW / _IOWR` 的含义
- [ ] 看懂 `demo_ioctl.h` 的命令号定义
- [ ] 看懂 `demo.c` 里的 `demo_ioctl()`
- [ ] 看懂 `test_ioctl.c` 如何打开 `/dev/demo`
- [ ] 完成一次 `SET`
- [ ] 完成一次 `GET`
- [ ] 验证非法命令和非法地址会返回错误码

---

## 3. 相关理论与原理

### 3.1 为什么要有 ioctl

`read/write` 适合顺序数据流。

但现实中的驱动经常需要做“控制类操作”，例如：

- 设置模式
- 读取寄存器值
- 打开/关闭某个能力
- 传递结构化命令

这类操作不适合单纯用 `read/write` 表达，所以 Linux 提供了：

```c
unlocked_ioctl
```

它的本质就是：

- 一个命令号 `cmd`
- 一个附带参数 `arg`

驱动根据 `cmd` 决定做什么。

### 3.2 ioctl 命令号为什么要放在头文件里

驱动和用户程序必须使用 **同一套命令定义**，否则：

- 驱动以为用户发的是 `SET`
- 用户实际编码出来的可能不是这个值

这就是为什么要有共享头文件：

```c
#include "demo_ioctl.h"
```

### 3.3 为什么不能直接解引用用户态指针

用户传来的地址属于用户空间，不可信，也不保证可访问。

所以内核不能直接：

```c
*user_ptr = kernel_value;
```

必须用：

- `copy_from_user()`：用户 -> 内核
- `copy_to_user()`：内核 -> 用户

这是 Linux 用户态/内核态隔离的基本规则。

### 3.4 Day02 的驱动逻辑

今天的驱动维护了一个简单内核变量：

```c
static int kernel_value = 0;
```

它模拟“设备内部状态”或“寄存器值”。

- `SET`：用户写入一个整数，驱动保存到 `kernel_value`
- `GET`：驱动把 `kernel_value` 返回给用户

虽然简单，但这已经是完整的用户态-内核态数据交互闭环。

---

## 4. 关键文件说明

- `demo.c`：驱动实现，重点在 `demo_ioctl()`
- `demo_ioctl.h`：驱动与用户态共用的 ioctl 协议头文件
- `test_ioctl.c`：用户态测试程序
- `Makefile`：同时编译模块和静态用户程序
- `build.sh`：启动最小实验环境

---

## 5. 执行步骤

### 5.1 进入目录

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day02
```

### 5.2 给脚本增加执行权限

```bash
chmod +x build.sh
```

### 5.3 运行实验

```bash
./build.sh
```

执行完成后会进入 QEMU shell。

---

## 6. 功能验证步骤

### 6.1 确认设备节点存在

```sh
ls -l /dev/demo
```

### 6.2 运行用户态测试程序

```sh
/bin/test_ioctl
```

预期：

- 程序能正常打开 `/dev/demo`
- 先执行 `SET`
- 再执行 `GET`
- 日志里能看到驱动打印 `Set value to ...` / `Get value ...`

### 6.3 观察内核日志

```sh
dmesg | tail -n 20
```

重点看：

- 模块是否加载成功
- `SET` 和 `GET` 是否进入对应分支
- 是否出现 `Unknown ioctl command`
- 是否出现 `-EFAULT`

### 6.4 可选：故意做错误测试

如果你想进一步理解错误码，可以后续把测试程序改成：

- 发一个未定义命令
- 或传非法指针

观察驱动如何返回：

- `-EINVAL`
- `-EFAULT`

---

## 7. 你应该掌握的知识点

### 问题 1：ioctl 和 read/write 的区别是什么？
ioctl 更适合做控制类命令；read/write 更适合数据流。

### 问题 2：为什么驱动和用户程序必须共用 `demo_ioctl.h`？
因为命令号必须一致，否则双方对同一个 `cmd` 的理解会不同。

### 问题 3：为什么要用 `copy_from_user / copy_to_user`？
因为内核不能直接访问用户态指针，必须通过受控拷贝接口搬运数据。

### 问题 4：Day02 的内核变量 `kernel_value` 代表什么？
它相当于设备状态的简化模拟，是后续寄存器、配置项、设备上下文的雏形。

---

## 8. 测试记录

### 本次验证状态

- [ ] `build.sh` 编译通过
- [ ] `/dev/demo` 存在
- [ ] `/bin/test_ioctl` 可运行
- [ ] `SET` 成功
- [ ] `GET` 成功
- [ ] `dmesg` 中可看到内核日志
- [ ] 错误路径已初步理解

### 建议保留的记录

```text
日期：
内核版本：5.15.10
实验目录：day02
执行命令：/bin/test_ioctl
SET 值：
GET 值：
dmesg 关键输出：
异常现象：
```

---

## 9. 今天最重要的一句话

Day02 的核心不是“学一个系统调用名字”，而是：

```text
理解驱动与用户态之间需要一套明确的 ABI 协议。
```

以后无论是字符设备、platform 驱动、PCIe 驱动，都会不断回到这个问题：

- 命令怎么设计
- 参数怎么传
- 错误码怎么返回
- 用户态如何验证


## QEMU 退出说明

当前实验默认使用 `-nographic` + `console=ttyS0`，所以 QEMU 会占用当前终端前台运行。这不是卡死，而是 QEMU 正在运行。

常用退出方式：

- 直接退出：`Ctrl+a`，然后按 `x`
- 进入 QEMU monitor：`Ctrl+a`，然后按 `c`，输入 `quit` 回车

不建议每次都用 `kill -9` 杀掉 `build.sh`，除非 QEMU 确实无响应。

