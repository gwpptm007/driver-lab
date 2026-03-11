# linux-driver-lab

Linux 驱动学习实验目录。

这里放的是每天的实验代码、测试脚本、README 和学习记录。
环境准备不在本目录处理，请先阅读：

- `../kernel-src/README.md`

---

## 目录概览

```text
linux-driver-lab/
├── README.md
├── docs/
├── day01/
├── day02/
├── day03/
├── day04/
├── day05/
├── day06/
├── day07/
├── day08/
└── day09/
```

---

## 环境依赖

本目录中的实验通常依赖：

- x86 内核构建目录：`../kernel-src/linux-5.15.10/build/x86`
- x86 内核镜像：`../kernel-src/linux-5.15.10/output/x86/bzImage`
- x86 BusyBox 安装目录：`../kernel-src/busybox-1.36.1/output/x86/_install`

如果要做 arm64 相关实验，则对应使用：

- arm64 内核构建目录：`../kernel-src/linux-5.15.10/build/arm64`
- arm64 内核镜像：`../kernel-src/linux-5.15.10/output/arm64/Image`
- arm64 BusyBox 安装目录：`../kernel-src/busybox-1.36.1/output/arm64/_install`

---

## 当前内容

### day01
基础字符设备驱动框架

### day02
加入 ioctl 和用户态测试

### day03
加入 sysfs

### day04
加入 debugfs

### day05
加入 waitqueue / workqueue

### day06
加入回归和压力测试脚本

### day07
整理 README 和阶段总结

### day08
platform_driver + probe/remove + devm 资源管理


### day09
Device Tree + of_match_table + reg/irq 解析


### day10
中断计数实现（request_irq + /proc 导出）
