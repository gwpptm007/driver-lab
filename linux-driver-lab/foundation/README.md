# Foundation 基础学习区

> day01 ~ day35 已收拢至此，作为驱动学习的基础实验区。

## 目录结构

```
foundation/
├── day01/ ~ day07/       W1：字符设备基础闭环
├── day08/ ~ day14/       W2：platform / DT / IRQ / regmap / ftrace
├── day15/ ~ day21/       W3：baseline / 裁剪 / perf / 回归 / 提交收口
├── day22/ ~ day28/       W4：PCIe 基本功作品线
└── day29/ ~ day35/       W5：DMA / mmap / bench / perf / ftrace / stability
```

## 学习路径

按周依次推进：
- **W1 (day01~07)**：字符设备基础 — miscdevice / ioctl / sysfs / debugfs / waitqueue / workqueue
- **W2 (day08~14)**：嵌入式通用套路 — platform_driver / Device Tree / IRQ / workqueue / regmap / ftrace
- **W3 (day15~21)**：工程化收口 — baseline 冻结 / 配置裁剪 / tracing 保留 / 回归自动化
- **W4 (day22~28)**：PCIe 基本功 — PCI 枚举 / BAR+MMIO / MSI / ivshmem 共享内存 / 用户工具
- **W5 (day29~35)**：DMA 与性能分析 — coherent DMA / mmap 零拷贝 / bench / perf / ftrace / 稳定性

## 环境依赖

外部依赖（位于项目根目录的 `../kernel-src/`）：
- `linux-5.15.10/build/x86` — x86 内核构建目录
- `linux-5.15.10/output/x86/bzImage` — x86 内核镜像
- `linux-5.15.10/build/arm64` — arm64 内核构建目录（arm64 实验用）
- `linux-5.15.10/output/arm64/Image` — arm64 内核镜像
- `busybox-1.36.1/output/x86/_install` — x86 BusyBox
- `busybox-1.36.1/output/arm64/_install` — arm64 BusyBox

## 各 day 入口

| 周 | 目录 | 入口 README |
|----|------|-------------|
| W1 | `day01/` ~ `day07/` | `day07/README.md` |
| W2 | `day08/` ~ `day14/` | `day14/README.md` |
| W3 | `day15/` ~ `day21/` | `day21/FINAL_SUBMISSION.md` |
| W4 | `day22/` ~ `day28/` | `day28/README.md` |
| W5 | `day29/` ~ `day35/` | `day35/README.md` |

## 运行任意 day

```bash
cd foundation/dayXX
chmod +x build.sh
./build.sh
```

## 与其他模块的关系

- `foundation/` — 基础实验，已跑通
- `w06_spi_i2c/` — 扩展学习（后续新建）
- `kernel-src/` — 内核 + BusyBox 源码与构建（外部依赖，不在本目录）
