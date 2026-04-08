# CLAUDE.md（中文版）

本文件为 Claude Code（claude.ai/code）在本代码库中工作时提供指导。

## 项目概述

这是一个 Linux 驱动学习实验室，按 35 天（day01-day35）组织，覆盖 5 周（W1-W5）的渐进式驱动开发：

- **W1（day01-07）**：字符设备基础（miscdevice、ioctl、sysfs、debugfs、waitqueue/workqueue、回归测试）
- **W2（day08-14）**：嵌入式驱动模式（platform_driver、Device Tree、IRQ、regmap、ftrace、启动检查清单）
- **W3（day15-21）**：内核裁剪与移植（defconfig、rootfs、启动链、回归测试）
- **W4（day22-28）**：PCIe 基础（ivshmem-doorbell 设备、PCI 枚举、BAR/MMIO、MSI 中断、用户工具）
- **W5（day29-35）**：DMA + 性能分析（QEMU EDU 设备、dma_alloc_coherent、mmap、性能基准测试、perf、ftrace、稳定性）

## 构建与测试命令

### 标准构建模式（x86）

每个 day 目录通常包含一个 `build.sh` 脚本，执行完整的实验流程：

```bash
cd linux-driver-lab/dayXX
chmod +x build.sh
./build.sh
```

build.sh 脚本执行以下步骤：
1. 编译驱动模块（`demo.ko`）
2. 编译用户态测试程序（`test.c`、`test_ioctl.c`）
3. 准备包含 BusyBox 的最小 rootfs
4. 生成 `/init` 脚本
5. 通过 `cpio | gzip` 打包 `rootfs.img`
6. 启动 QEMU

### 手动编译模块

```bash
make KDIR=/path/to/kernel/build/dir clean
make KDIR=/path/to/kernel/build/dir
```

### QEMU 退出

当 QEMU 使用 `-nographic` + `console=ttyS0` 运行时，退出方式：
- `Ctrl+a`，然后按 `x` 直接退出
- `Ctrl+a`，然后按 `c` 进入 QEMU 监视器，输入 `quit`

## 环境依赖

代码库期望的兄弟目录结构：

```
driver-lab/
├── linux-driver-lab/       # 本代码库
└── kernel-src/
    ├── linux-5.15.10/
    │   ├── build/x86/      # x86 内核构建目录
    │   ├── build/arm64/    # arm64 内核构建目录
    │   └── output/
    │       ├── x86/bzImage
    │       └── arm64/Image
    └── busybox-1.36.1/
        └── output/
            ├── x86/_install/bin/busybox
            └── arm64/_install/bin/busybox
```

关键路径（可通过环境变量配置）：
- `KDIR`：内核构建目录（`../kernel-src/linux-5.15.10/build/x86` 或 `/build/arm64`）
- `KERNEL_IMG`：内核镜像（`../kernel-src/linux-5.15.10/output/x86/bzImage` 或 `/arm64/Image`）
- `BUSYBOX_INSTALL`：BusyBox 安装目录（`../kernel-src/busybox-1.36.1/output/x86/_install`）

### 交叉编译（ARM64）

对于 ARM64 实验（day08-14），设置交叉编译器变量：

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
./build.sh
```

所需工具：`qemu-system-aarch64`、`dtc`（device-tree-compiler）、`aarch64-linux-gnu-gcc`

## 代码架构

### 标准 Day 目录结构

```
dayXX/
├── demo.c                 # 驱动主源码
├── Makefile               # 模块编译规则
├── build.sh               # 完整实验脚本
├── test.c / test_ioctl.c  # 用户态测试程序
├── demo_ioctl.h           # ioctl 命令定义
├── README.md              # 当日学习目标与任务
├── START_HERE.md          # 快速入门指南（后续周）
├── docs/                  # 详细文档（后续周）
├── records/               # 原始证据/日志（后续周）
├── output/                # 处理后的结果（后续周）
└── rootfs/                # 临时 rootfs 目录（生成）
```

### 驱动模式

**早期天数（01-07）**：使用 `miscdevice` 实现简单字符设备：
- `misc_register()` 注册设备
- `struct file_operations` 实现 VFS 回调
- `/dev/demo` 设备节点自动创建

**W2（08-14）**：平台驱动模型：
- `platform_driver_register()` 配合 `probe`/`remove`
- `of_match_ptr` 用于 Device Tree 匹配
- `of_property_read_*` 解析寄存器/IRQ
- `request_irq()` 配合顶半部处理程序
- `workqueue` 实现底半部延迟工作
- `regmap` 封装寄存器访问

**W4（22-28）**：PCI 驱动模型：
- `pci_register_driver()` 配合 `pci_driver` 结构体
- `pci_enable_device()`、`pci_request_regions()`、`pci_iomap()`
- BAR 资源管理与 MMIO 访问
- `pci_alloc_irq_vectors()`、`request_irq()` 用于 MSI
- 用户工具进行设备交互

**W5（29-35）**：DMA 与性能：
- `dma_set_mask_and_coherent()`
- `dma_alloc_coherent()` 分配 DMA 缓冲区
- `mmap()` 文件操作实现用户态零拷贝访问
- 吞吐量/延迟性能基准测试
- `perf record/report` 分析热点
- `ftrace function_graph` 分析调用路径

## 关键实现细节

### 设备树注入（day08-14）

ARM64 实验使用 `inject_virt_dt.py` 注入测试设备片段：
1. 导出 QEMU virt 基础 DTB：`qemu-system-aarch64 -machine virt,dumpdtb=virt-base.dtb`
2. 反编译：`dtc -I dtb -O dts -o virt-base.dts`
3. 注入片段：`python inject_virt_dt.py --input virt-base.dts --fragment demo.fragment.dtsi --output virt-new.dts`
4. 重新编译：`dtc -I dts -O dtb -o virt-new.dtb`

### 证据收集模式

从 day17 开始，维护有序的证据：
- `records/`：原始输出（dmesg、lspci、trace 日志、基准测试结果）
- `output/`：处理后的摘要和分析
- 每次运行生成带时间戳的记录（如 `compare-20260314-231137.md`）

### Rootfs 生成

始终使用静态链接的 BusyBox 构建最小 rootfs。用 `file` 命令检查：
```bash
file path/to/busybox  # 不应显示 "dynamically linked"
```

Rootfs 打包：
```bash
cd rootfs
find . | cpio -o -H newc | gzip -9 > ../rootfs.img
```

## 测试与回归

### 基本验证步骤

1. 检查模块加载：`lsmod | grep demo`
2. 检查设备节点：`ls -l /dev/demo`
3. 验证功能：`echo hello > /dev/demo`（因日期而异）
4. 检查内核日志：`dmesg | tail`
5. 确认无 Oops/warning/panic

### 回归测试

- Day06 包含 insmod/rmmod 循环和并发压力测试的回归脚本
- W3 专注于基准冻结和自动化回归
- 使用各日期中的脚本进行标准化测试

## 重要开发注意事项

### 构建脚本设计

`build.sh` 脚本设计为每个日期实验的单一入口点。它们：
- 处理相对路径解析以支持不同代码库布局
- 支持环境变量覆盖（KDIR、KERNEL_IMG、BUSYBOX_INSTALL）
- 自动检测和使用静态链接的 BusyBox
- 提供清晰的错误信息和依赖缺失提示

### 模块卸载安全

始终实现对称的资源管理：
- 在 `probe/init` 中分配的必须在 `remove/exit` 中释放
- 顺序：按分配的反向顺序释放
- 用 `insmod/rmmod` 循环（200-1000 次）测试稳定性

### 代码组织

- 驱动源码：`demo.c` 或描述性名称（`demo_pdrv.c`、`demo_irq.c`）
- ioctl 定义：`demo_ioctl.h` 与用户态测试程序共享
- 测试程序：用 `gcc -static` 编译以兼容最小 rootfs

### 新用户阅读顺序

理解项目结构和学习路径的顺序：
1. `linux-driver-lab/README.md` - 整体路线图
2. `docs/FILE_GUIDE.md` - 文件用途说明
3. `docs/ROADMAP.md` - 按周学习目标
4. 各日期的 `README.md` - 具体主题
5. 后续周：`START_HERE.md` → `docs/01_overview.md` 等

### W4-W5 整合

W4（PCIe 基础，ivshmem-doorbell）和 W5（DMA/性能，QEMU EDU）设计为互补：
- W4：PCI 枚举、BAR/MMIO、MSI 中断、remove 对称性
- W5：DMA 一致性缓冲区、mmap 零拷贝、性能基准测试、perf/ftrace 分析、稳定性

W3 的证据收集模式应延续到 W4/W5。
