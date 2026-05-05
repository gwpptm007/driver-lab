# CLAUDE.md（中文版）

本文件为 Claude Code（claude.ai/code）在本代码库中工作时提供指导。

## 项目概述

Linux 驱动学习实验室，包含多条渐进式学习路径：

| 路径 | 内容 | 构建方式 |
|------|------|----------|
| `foundation/` | day01-day35（W1-W5）：miscdevice → platform/IRQ → PCIe → DMA/性能 | 每日 `build.sh` |
| `netdev/` | stage00-stage14：net_device/skb/NAPI → virtio-net → XDP | 每 stage `make` |
| `track-dpdk/` | DPDK 用户态 fastpath（vmxnet3 PMD → vhost-user → virtio-user → fastpath C 应用） | `meson + ninja` |
| `track-real-driver/` | virtio-net 源码深潜 | （分析为主） |
| `track-virtual-net/` | tap/bridge/vhost 机制 | （分析为主） |

## 构建与测试命令

### Foundation Days（W1-W5）

```bash
cd linux-driver-lab/foundation/dayXX
chmod +x build.sh
./build.sh
```

`build.sh` 执行：驱动编译 → rootfs 准备 → QEMU 启动。QEMU 退出：`Ctrl+a x`（退出）或 `Ctrl+a c` 然后 `quit`（监视器）。

### 手动编译模块

```bash
make KDIR=/path/to/kernel/build/dir clean
make KDIR=/path/to/kernel/build/dir
```

### Track-DPDK（meson + ninja）

```bash
cd track-dpdk/project-user-space-fastpath
./scripts/01_build_app.sh        # meson 编译
sudo ./scripts/03_run_fastpath_single_port.sh  # 运行
```

### Netdev Stages

```bash
cd linux-driver-lab/netdev/stageXX
make KDIR=/path/to/kernel/build/dir
```

## 环境依赖

```
driver-lab/
├── linux-driver-lab/       # 本代码库
└── kernel-src/             # 外部（兄弟目录）
    ├── linux-5.15.10/
    │   ├── build/x86/      # KDIR 内核构建目录
    │   └── output/x86/bzImage  # KERNEL_IMG
    └── busybox-1.36.1/output/x86/_install  # BUSYBOX_INSTALL
```

关键环境变量：`KDIR`、`KERNEL_IMG`、`BUSYBOX_INSTALL`。

### ARM64 交叉编译（W2、stage06）

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
./build.sh
```

所需工具：`qemu-system-aarch64`、`dtc`、`aarch64-linux-gnu-gcc`。

## 代码架构

### Foundation Day 目录结构

```
dayXX/
├── demo.c                 # 驱动源码
├── Makefile               # 模块编译规则
├── build.sh               # 完整实验脚本
├── test.c / test_ioctl.c  # 用户态测试程序
├── demo_ioctl.h           # 共享 ioctl 定义
├── README.md              # 学习目标
├── docs/                  # 详细文档
├── records/               # 原始证据/日志（带时间戳）
└── output/                # 处理后的结果
```

### 驱动模式（按周）

| 周 | 天数 | 模式 |
|----|------|------|
| W1 | 01-07 | `miscdevice` + `misc_register()` |
| W2 | 08-14 | `platform_driver` + Device Tree + `of_property_read_*` |
| W4 | 22-28 | `pci_register_driver` + BAR/MMIO + MSI |
| W5 | 29-35 | `dma_alloc_coherent` + `mmap` + perf/ftrace |

### Netdev Stage 结构

```
stageXX/
├── demo.c                 # Netdev 驱动源码
├── Makefile               # 使用内核构建系统
├── test.c                 # 用户态测试（可选）
├── README.md              # Stage 学习目标
└── scripts/               # 辅助脚本（可选）
```

### Track-DPDK 结构

```
track-dpdk/
├── lab-vmxnet3-testpmd/   # DPDK 环境验证
├── lab-vhost-user-basic/  # vhost-user socket 实验
├── lab-virtio-user-vhost/ # virtio-user + vhost-user 配对
├── lab-dpdk-l2-forwarding/# L2fwd C 应用（meson 构建）
├── project-user-space-fastpath/  # fastpath C 应用（meson 构建）
│   └── app/main.c         # EAL 初始化、mbuf pool、rx_burst/tx_burst、分类/重写
└── docs/                  # 整合后的文档
```

### 设备树注入（W2 / stage05-06）

ARM64 实验使用 `inject_virt_dt.py`：
1. `qemu-system-aarch64 -machine virt,dumpdtb=virt-base.dtb`
2. `dtc -I dtb -O dts -o virt-base.dts`
3. `python inject_virt_dt.py --input virt-base.dts --fragment demo.fragment.dtsi --output virt-new.dts`
4. `dtc -I dts -O dtb -o virt-new.dtb`

## 关键实现细节

### 模块卸载安全

对称资源管理：按分配的反向顺序释放。用 `insmod/rmmod` 循环（200-1000 次）测试稳定性。

### 证据收集

从 day17/W3 开始：
- `records/`：原始输出（dmesg、lspci、trace 日志、性能基准）
- `output/`：处理后的摘要，带时间戳（如 `compare-20260314-231137.md`）

### Rootfs 生成

必须使用静态链接的 BusyBox。检查：`file busybox` 显示 "statically linked"。打包：`find . | cpio -o -H newc | gzip -9 > ../rootfs.img`

## 文档结构（`docs/`）

整合后 docs 目录组织如下：

| 文件 | 内容 |
|------|------|
| `01_PROGRAMS.md` | 当前阶段、track 定位、下一步 |
| `02_EXPERT_REVIEW.md` | 专家评审结论与执行计划 |
| `03_PROGRESS.md` | 当前进度与完成度矩阵 |
| `04_ARCHITECTURE.md` | 架构分层与完成度矩阵 |
| `05_START_HERE.md` | 快速入门与 GitHub 使用说明 |
| `06_TEST_GUIDE.md` | 测试记录模板 |
| `07_FOUNDATION_REVIEWS.md` | W1-W5 周评审与计划汇总 |

### 推荐阅读顺序

1. `linux-driver-lab/START_HERE_CURRENT.md` - 当前状态
2. `linux-driver-lab/README.md` - 整体路线图
3. `foundation/README.md` - day01-35 学习路径
4. `netdev/README.md` - stage00-14 网络驱动主线
5. `track-dpdk/README.md` - DPDK 用户态网络
6. `docs/03_PROGRESS.md` - 当前进度与开放项
