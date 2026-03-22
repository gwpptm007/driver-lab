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
├── day01/ ~ day14/   字符设备 / platform_driver / DT / regmap / irq / ftrace 基础线
├── day15/ ~ day18/   配置裁剪 / profile / perf 能力收口
├── day19/ ~ day21/   回归套件 / 总结 / 最终交付文档线
└── day22/ ~ day35/   PCIe 作品线（W4-W5）
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

### day01 ~ day07
字符设备基础、ioctl/sysfs/debugfs、waitqueue/workqueue、回归脚本与阶段总结。

### day08 ~ day14
platform_driver、Device Tree、request_irq、中断计数、bottom half、regmap、ftrace 和 bring-up checklist。

### day15 ~ day18
裁剪、profile、perf/tracing 能力保持、结果比对与交付收口。

### day19 ~ day21
回归方案、总结文档、最终提交版报告与脚本化输出。

### day22 ~ day28（W4）
PCIe 基本功学习线：  
QEMU PCI 设备选型（ivshmem）→ day22 设备可见性 → day23 `pci_driver` 骨架与 BAR 资源接管 → day24 MMIO/共享内存协议 → day25 消息中断向量 → day26 用户态工具 → day27 remove/循环卸载 → day28 证据归档。

### day29 ~ day35（W5）
DMA/性能分析学习线：  
DMA-capable 设备（QEMU EDU）→ `dma_alloc_coherent` → `mmap` 零拷贝 → bench → perf → ftrace → 稳定性 → 性能与风险报告。

---

## W4-W5 当前推荐路线

- **W4 使用 ivshmem-doorbell**
  - 先把 PCI 设备发现、BAR、MMIO、共享内存、消息中断、用户态接口、remove 对称释放这套基本功学透。
- **W5 使用 DMA-capable 设备（推荐 QEMU EDU）**
  - 再把 `dma_alloc_coherent`、DMA 校验、`mmap`、bench、perf、ftrace、并发与稳定性做成“真闭环”。

也就是说：
- W4 是 PCI 基本功线；
- W5 是 DMA/性能分析线；
- 两段组合成最终的 PCIe 作品。

---

## 建议阅读顺序

1. `docs/W3_REVIEW.md`
2. `docs/W3_BASELINE_AND_REGRESSION_GUIDE.md`
3. `docs/W4_PCIE_PLAN.md`
4. `docs/W5_DMA_PERF_PLAN.md`
5. `day22/START_HERE.md`
6. `day29/START_HERE.md`
7. 之后按 day22 → day35 顺序推进
