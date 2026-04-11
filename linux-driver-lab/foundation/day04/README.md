# Day04：debugfs 状态快照、日志开关与可观测性

## 1. 学习目标

Day04 的关键词不是“多一个接口”，而是 **可观测性**。

今天要完成：

- 理解 `debugfs` 的定位
- 在 `/sys/kernel/debug/demo_debug/` 下导出调试节点
- 通过 `status` 读取驱动内部状态快照
- 通过 `log_level` 动态控制日志输出
- 理解为什么 debugfs 更适合开发调试，而不是正式 ABI

这一天和你后面做 `perf / ftrace / 采证` 的思维是一脉相承的。

---

## 2. 今天的任务清单

- [ ] 成功执行 `build.sh`
- [ ] 确认系统里已挂载 debugfs
- [ ] 确认 `/sys/kernel/debug/demo_debug/status` 存在
- [ ] 确认 `/sys/kernel/debug/demo_debug/log_level` 存在
- [ ] 能读取状态快照
- [ ] 能动态修改 `log_level`
- [ ] 能观察到关闭设备后 ioctl 被拒绝
- [ ] 知道 debugfs 和 sysfs 的区别

---

## 3. 相关理论与原理

### 3.1 什么是 debugfs

`debugfs` 是 Linux 提供的一个调试文件系统，典型挂载点是：

```text
/sys/kernel/debug
```

它的特点是：

- 用起来很灵活
- 非常适合导出临时调试信息
- 适合开发阶段查看内部状态
- 不强调长期 ABI 稳定性

你可以把它理解成：

```text
给驱动开发者准备的“调试窗口”。
```

### 3.2 debugfs 和 sysfs 的区别

#### sysfs

更适合：

- 稳定属性
- 配置项
- 状态项
- 面向用户或运维的简单接口

#### debugfs

更适合：

- 调试快照
- 内部统计
- 临时调试开关
- 开发过程中的辅助观察

简单记忆：

- `sysfs`：偏正式、偏属性
- `debugfs`：偏调试、偏开发

### 3.3 Day04 的设备结构为什么更像“工程化版本”

Day04 开始引入：

```c
struct demo_device
```

把这些状态收敛到一个设备对象里：

- `enable`
- `counter`
- `log_level`
- `debug_root`
- `cdev/class/device/dev_id`

这比 Day02/Day03 全局变量散放的方式更接近真实驱动。

### 3.4 status 节点的原理

Day04 的 `status` 通过：

- 组织一段格式化字符串
- 再通过 `simple_read_from_buffer()` 返回给用户

这是一种很常见的“文本快照导出方式”。

用户态执行：

```sh
cat /sys/kernel/debug/demo_debug/status
```

看到的其实是某一时刻驱动内部状态的文本快照。

### 3.5 log_level 节点的原理

Day04 使用：

```c
debugfs_create_u32("log_level", 0644, ...)
```

这意味着：

- 读：可以看到当前日志级别
- 写：可以直接修改内核里的 `u32 log_level`

配合：

```c
if (g_demo_dev->log_level > 0)
    pr_info_ratelimited(...)
```

就实现了一个很实用的“可动态开关日志”的调试能力。

---

## 4. 关键文件说明

- `demo.c`：cdev + sysfs + debugfs 的综合版本
- `test.c`：用户态 ioctl 测试程序
- `build.sh`：会自动在 `/init` 里挂载 debugfs
- `Makefile`：模块与用户程序编译规则

---

## 5. 执行步骤

### 5.1 进入目录

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day04
```

### 5.2 给脚本增加执行权限

```bash
chmod +x build.sh
```

### 5.3 运行实验

```bash
./build.sh
```

执行完成后进入 QEMU shell。

---

## 6. 功能验证步骤

### 6.1 确认 debugfs 已挂载

```sh
mount
```

预期：应能看到 `debugfs` 挂在 `/sys/kernel/debug`。

### 6.2 查看 debugfs 目录

```sh
ls -l /sys/kernel/debug/demo_debug
```

预期：应看到至少两个节点：

- `status`
- `log_level`

### 6.3 读取状态快照

```sh
cat /sys/kernel/debug/demo_debug/status
```

预期：看到类似：

- Device Enable
- IOCTL Count
- Debug Log Lvl
- Kernel Time

### 6.4 查看并修改日志级别

```sh
cat /sys/kernel/debug/demo_debug/log_level
echo 1 > /sys/kernel/debug/demo_debug/log_level
cat /sys/kernel/debug/demo_debug/log_level
```

预期：

- 初始值可能为 `0`
- 修改后变成 `1`

### 6.5 观察 ioctl 行为和日志输出

执行测试程序：

```sh
/bin/test
```

然后查看日志：

```sh
dmesg | tail -n 20
```

预期：

- 如果 `log_level > 0`，能看到 `IOCTL handled`
- `counter` 会增加

### 6.6 配合 sysfs 验证拒绝路径

先关闭设备：

```sh
echo 0 > /sys/class/demo_day04/demo_day04/enable
```

再执行：

```sh
/bin/test
```

预期：

- 返回 `EPERM`
- 若日志开关打开，可看到 `Device disabled, IOCTL rejected!`

---

## 7. 你应该掌握的知识点

### 问题 1：为什么 Day04 不把所有状态都继续放在 sysfs？
因为有些信息更偏开发调试，例如状态快照、调试开关、临时统计，更适合放 debugfs。

### 问题 2：为什么 Day04 比 Day03 更像真实驱动？
因为它开始收敛到设备对象 `struct demo_device`，而不是继续堆全局变量。

### 问题 3：为什么日志要做动态开关？
因为驱动调试时需要看细节，但稳定性测试和性能测试时不希望大量日志扰动系统，所以要能动态控制。

### 问题 4：`pr_info_ratelimited()` 的意义是什么？
防止高频路径疯狂刷日志，把串口/QEMU 输出刷爆，也避免过度扰动行为。

---

## 8. 测试记录

### 本次验证状态

- [ ] `build.sh` 运行成功
- [ ] debugfs 挂载成功
- [ ] `status` 可读
- [ ] `log_level` 可读写
- [ ] `/bin/test` 可触发 ioctl
- [ ] `counter` 能增长
- [ ] 禁用后 ioctl 被拒绝
- [ ] 日志开关行为符合预期

### 建议保留的记录

```text
日期：
内核版本：5.15.10
实验目录：day04
status 输出：
log_level 修改前后：
测试程序输出：
dmesg 关键日志：
异常现象：
```

---

## 9. 当前实现的注意点

Day04 的整体结构比前几天更工程化，但你也要知道当前版本还有一些“教学版”限制：

- ioctl 语义目前更偏“计数示例”，没有完整恢复 Day02 的真实 `SET/GET` 搬运逻辑
- 初始化失败回滚路径还不够完整
- 并发保护还没引入

这不是错误，而是说明接下来的 Day05/Day06 还有明确的演进空间。

---

## 10. 今天最重要的一句话

Day04 的核心价值不是“再多两个文件”，而是：

```text
驱动开始具备真正的调试可观测性。
```

这一步打好后，后面你再学 `waitqueue / perf / ftrace / PCIe bench` 时，思维会明显顺很多。


## QEMU 退出说明

当前实验默认使用 `-nographic` + `console=ttyS0`，所以 QEMU 会占用当前终端前台运行。这不是卡死，而是 QEMU 正在运行。

常用退出方式：

- 直接退出：`Ctrl+a`，然后按 `x`
- 进入 QEMU monitor：`Ctrl+a`，然后按 `c`，输入 `quit` 回车

不建议每次都用 `kill -9` 杀掉 `build.sh`，除非 QEMU 确实无响应。

