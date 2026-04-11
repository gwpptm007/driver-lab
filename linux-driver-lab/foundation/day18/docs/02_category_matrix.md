# Day18 分类矩阵

这份表把 day18 的分类裁剪拆成 **required / platform / debug / perf / trim** 五类。
它不是替代 `.config`，而是给学习、复盘、汇报时提供一张可解释的总览表。

## required

| symbol | expected | why_keep_or_trim | source_fragment |
|---|---|---|---|
| CONFIG_BINFMT_ELF | y | 用户态 /init 与 BusyBox 可执行文件依赖 ELF | 10_required.fragment |
| CONFIG_BLK_DEV_INITRD | y | 当前通过 initramfs 启动 | 10_required.fragment |
| CONFIG_DEVTMPFS | y | /dev 自动节点依赖 | 10_required.fragment |
| CONFIG_DEVTMPFS_MOUNT | y | guest 启动时自动挂载 devtmpfs | 10_required.fragment |
| CONFIG_PROC_FS | y | 采样脚本依赖 /proc | 10_required.fragment |
| CONFIG_SYSFS | y | 设备模型与 sysfs 依赖 | 10_required.fragment |
| CONFIG_TMPFS | y | /tmp 与临时文件依赖 | 10_required.fragment |
| CONFIG_MODULES | y | 需要 insmod demo_regmap.ko | 10_required.fragment |

## platform

| symbol | expected | why_keep_or_trim | source_fragment |
|---|---|---|---|
| CONFIG_OF | y | QEMU virt + DT 注入主链依赖 | 20_platform.fragment |
| CONFIG_OF_IRQ | y | 从 DT 解析 irq 依赖 | 20_platform.fragment |
| CONFIG_SERIAL_AMBA_PL011 | y | QEMU virt 串口控制台依赖 | 20_platform.fragment |
| CONFIG_SERIAL_AMBA_PL011_CONSOLE | y | console=ttyAMA0 依赖 | 20_platform.fragment |
| CONFIG_ARM_GIC | y | virt 平台中断控制器依赖 | 20_platform.fragment |
| CONFIG_ARM_GIC_V3 | y | arm64 virt 常用 GICv3 | 20_platform.fragment |
| CONFIG_IRQ_DOMAIN | y | IRQ domain 主链依赖 | 20_platform.fragment |
| CONFIG_REGMAP | y | day12/day17 demo_regmap 主链依赖 | 20_platform.fragment |
| CONFIG_REGMAP_MMIO | y | regmap-mmio 访问寄存器依赖 | 20_platform.fragment |

## debug

| symbol | expected | why_keep_or_trim | source_fragment |
|---|---|---|---|
| CONFIG_DEBUG_FS | y | debugfs 观测入口 | 30_debug.fragment |
| CONFIG_TRACEPOINTS | y | tracing 基础能力 | 30_debug.fragment |
| CONFIG_TRACING | y | ftrace 主开关 | 30_debug.fragment |
| CONFIG_FTRACE | y | ftrace 主功能 | 30_debug.fragment |
| CONFIG_FUNCTION_GRAPH_TRACER | y | day13/day17 IRQ 路径跟踪依赖 | 30_debug.fragment |
| CONFIG_FRAME_POINTER | y | 回溯与可读性更好 | 30_debug.fragment |
| CONFIG_IKCONFIG | y | 配置可追溯性 | 30_debug.fragment |
| CONFIG_IKCONFIG_PROC | y | /proc/config.gz 取证方便 | 30_debug.fragment |

## perf

| symbol | expected | why_keep_or_trim | source_fragment |
|---|---|---|---|
| CONFIG_PERF_EVENTS | y | perf stat/list 基础能力 | 40_perf.fragment |
| CONFIG_HW_PERF_EVENTS | y | perf 事件框架能力 | 40_perf.fragment |

## trim

| symbol | expected | why_keep_or_trim | source_fragment |
|---|---|---|---|
| CONFIG_PCI | n | 当前实验路径不依赖 PCI | 90_trim_day18.fragment |
| CONFIG_SCSI | n | initramfs 启动不依赖 SCSI 磁盘 | 90_trim_day18.fragment |
| CONFIG_NET | n | 串口采样链与 demo_regmap 不依赖网络 | 90_trim_day18.fragment |

