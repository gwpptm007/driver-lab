# linux-driver-lab

Linux 驱动学习实验主目录。

这里放的是 day01 ~ day35 的：

- 驱动代码
- 构建脚本
- QEMU / rootfs 配套脚本
- records 证据归档
- 阶段总结文档

当前这套目录已经形成了比较完整的 **W1 ~ W5 学习闭环**。

---

## 1. 先看哪里

建议先看：

1. `START_HERE_CURRENT.md`
2. `docs/CURRENT_PROJECT_REVIEW.md`
3. `docs/PROGRESS.md`

如果你是第一次接触本仓库，再补：

4. `../kernel-src/README.md`
5. 本文件（`README.md`）

---

## 2. 当前目录概览

```text
linux-driver-lab/
├── README.md
├── START_HERE_CURRENT.md             当前仓库总入口（本次新整理）
├── POST_DAY35_LEARNING_ROADMAP.md    day35 后续学习路线草案
├── docs/
│   ├── CURRENT_PROJECT_REVIEW.md     当前项目状态深度整理（本次新整理）
│   ├── PROGRESS.md                   当前进度与阶段结论（本次更新）
│   ├── ROADMAP.md
│   ├── W1_REVIEW.md
│   ├── W3_REVIEW.md
│   ├── W4_PCIE_PLAN.md
│   └── W5_DMA_PERF_PLAN.md
├── day01/  ~ day07/                  W1：字符设备基础闭环
├── day08/  ~ day14/                  W2：platform / DT / IRQ / regmap / ftrace
├── day15/  ~ day21/                  W3：baseline / 裁剪 / perf / 回归 / 提交收口
├── day22/  ~ day28/                  W4：PCIe 基本功作品线
└── day29/  ~ day35/                  W5：DMA / mmap / bench / perf / ftrace / stability
```

---

## 3. 当前阶段划分

### W1：day01 ~ day07
主题：字符设备基础闭环

覆盖：
- miscdevice / file_operations
- ioctl
- sysfs / debugfs
- waitqueue / workqueue
- 回归脚本与并发压测

### W2：day08 ~ day14
主题：嵌入式通用驱动套路

覆盖：
- platform_driver
- Device Tree 匹配与资源解析
- request_irq
- workqueue bottom-half
- regmap
- function_graph
- bring-up checklist

### W3：day15 ~ day21
主题：工程化收口

覆盖：
- baseline 冻结
- 配置裁剪
- tracing / perf 能力保留
- 对比与回归自动化
- 阶段总结与提交物生成

### W4：day22 ~ day28
主题：PCIe 基本功作品线

覆盖：
- PCI 枚举与 `lspci -vv`
- `pci_driver` 骨架
- BAR / MMIO / 共享内存协议
- MSI / 用户态接口
- remove / 循环装卸 / 证据收口

### W5：day29 ~ day35
主题：DMA 与性能分析作品线

覆盖：
- coherent DMA
- `mmap` 零拷贝
- bench
- perf / function_graph
- 稳定性与错误注入
- 阶段性能与风险报告

---

## 4. 当前项目的一句话定位

> 这不是“学几个驱动 API”的目录，而是一套从最小驱动骨架，一直走到 PCIe / DMA / perf / 稳定性报告的实验型驱动学习项目。

---

## 5. 当前最适合评审的入口

### 看 W3 收口
- `day21/FINAL_SUBMISSION.md`

### 看 W4 收口
- `day28/README.md`

### 看 W5 收口
- `day35/README.md`

### 看整体判断
- `docs/CURRENT_PROJECT_REVIEW.md`
- `docs/PROGRESS.md`

---

## 6. 环境依赖

本目录中的实验通常依赖：

- x86 内核构建目录：`../kernel-src/linux-5.15.10/build/x86`
- x86 内核镜像：`../kernel-src/linux-5.15.10/output/x86/bzImage`
- x86 BusyBox 安装目录：`../kernel-src/busybox-1.36.1/output/x86/_install`

如果做 arm64 相关实验，则通常使用：

- arm64 内核构建目录：`../kernel-src/linux-5.15.10/build/arm64`
- arm64 内核镜像：`../kernel-src/linux-5.15.10/output/arm64/Image`
- arm64 BusyBox 安装目录：`../kernel-src/busybox-1.36.1/output/arm64/_install`

---

## 7. 当前建议阅读顺序

### 情况 A：你想从头学
按 `day01 -> day35` 顺序推进。

### 情况 B：你想快速看当前完成度
按下面顺序：

1. `START_HERE_CURRENT.md`
2. `docs/CURRENT_PROJECT_REVIEW.md`
3. `day21/FINAL_SUBMISSION.md`
4. `day28/README.md`
5. `day35/README.md`

### 情况 C：你想开始做代码评审
优先看：

- W1 / W2 的接口和基础套路
- W4 / W5 的 records、脚本、输出物
- `docs/PROGRESS.md` 中的“当前开放项”
