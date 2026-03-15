# W4 PCIe 作品线：先把 PCI 基本功学透

## 1. 当前推荐方案总览

W4/W5 最终作品标题保持不变：

**W4-W5 PCIe 作品（BAR + MSI + DMA + mmap + bench）**

但实现路线不再强行用一个设备从头走到底，而是拆成两段：

- **W4（day22 ~ day28）**：使用 `ivshmem-doorbell`
- **W5（day29 ~ day35）**：切到 DMA-capable PCI 设备，推荐 `QEMU EDU`

这份文档先讲清楚 **W4 为什么这样设计、W4 具体要做到什么、W4 和 W5 怎么衔接**。

---

## 2. 为什么 W4 先用 ivshmem

W4 的目标不是一次性把 DMA、性能优化、稳定性报告全揉进去，而是先把 PCI 驱动里最基础、最容易讲清楚的一圈能力真正学透：

- PCI 设备枚举
- `lspci -vv` 读信息
- `pci_driver` 生命周期
- BAR 资源申请与映射
- `readl` / `writel`
- 中断向量申请与处理中断
- 用户态工具触发与查询
- remove 对称释放
- 循环装卸稳定性

`ivshmem-doorbell` 非常适合这个阶段，因为它把 PCI + BAR + 共享内存 + doorbell interrupt 放在一个很直观的设备模型里，学习反馈快、路径短、证据也容易留。

---

## 3. 为什么 W4 不急着做“真 DMA”

如果在 W4 就强行把“DMA 真闭环”塞进来，学习会立刻分心到：

- DMA 一致性
- buffer 组织
- 数据流方向
- DMA 完成通知
- `mmap` 和用户态零拷贝
- bench/perf/ftrace

这会让前面最该学透的 PCI 基本功反而学不扎实。

所以当前方案强调的是：

> **W4 先把 PCI 基本功吃透，W5 再把 DMA/性能分析做真。**

这不是绕路，而是更适合学习和交付。

---

## 4. W4 学完后，你应该真正会什么

如果 W4 做顺了，最后你至少应该能把下面这段话讲顺：

1. 我知道 PCI 设备在 guest 里怎么被枚举出来
2. 我会看 `lspci -vv`，能把 BAR、capability、irq 信息讲清楚
3. 我会写最小 `pci_driver`，知道 probe/remove 怎么对称
4. 我知道 `enable_device / request_regions / pci_iomap` 的作用顺序
5. 我会做 MMIO 读写和简单共享内存协议
6. 我会用 `pci_alloc_irq_vectors + request_irq` 申请并处理中断
7. 我会做用户态工具、状态查询和明确错误码
8. 我会做装卸循环与原始证据归档

这套能力已经是很像岗位里的“PCI 驱动基本功”了。

---

## 5. W4 整体实施路线

### D22：平台准备 + 设备可见

目标：

- 拉起 PCI / MSI 相关内核配置
- 准备 guest 中 `lspci`
- 在 QEMU 中挂上 `ivshmem-doorbell`
- 完成 `lspci -vv` 归档

关键产物：

- `records/lspci-vv.txt`
- `records/dmesg-pci.txt`
- `output/day22-summary.md`

### D23：`pci_driver` 骨架

目标：

- `pci_register_driver`
- `pci_enable_device`
- `pci_request_regions`
- `pci_iomap`
- probe 成功打印 BAR 信息
- remove 对称释放

### D24：MMIO / 共享内存协议

目标：

- 用 `readl` / `writel` 读写 BAR 寄存器
- 建立最小共享区协议
- 完成用户态基本读写验证

### D25：中断

目标：

- `pci_alloc_irq_vectors`
- `request_irq`
- 触发设备消息中断
- `/proc/interrupts` 计数增长
- 驱动内部 irq 计数同步增长

### D26：用户态工具

目标：

- 统一 `ioctl` 或 `read/write` 接口
- 支持状态查询、触发、错误返回
- 把“能跑”变成“可复现工具链”

### D27：卸载与循环

目标：

- BAR / irq / cdev / 资源释放对称
- `insmod/rmmod` 200 次通过
- 无 oops / hang / 明显泄漏痕迹

### D28：README 与证据归档

目标：

- 设备信息
- 跑法
- 原始证据
- 验收说明
- 风险与已知限制

---

## 6. W4 建议目录职责

建议把 W4 理解为“一条完整作品线的前半段”，而不是若干零散实验日。

### day22 ~ day28 每天都建议保留

- `README.md`：今天做什么、为什么做
- `START_HERE.md`：当天阅读/执行入口
- `docs/01_plan.md`：实施路线
- `docs/02_acceptance.md`：验收口径
- `records/`：原始输出
- `output/`：整理结论

### W4 额外建议统一保留

- QEMU 启动参数样例
- PCI/MSI 相关 config checklist
- 用户态工具接口文档
- irq/stat 口径说明

---

## 7. W4 和 W5 怎么衔接

W4 完成后，建议把下面这些东西继续复用到 W5：

- records/output 组织方式
- 用户态工具风格
- 错误码与日志格式
- bench/report 的归档习惯
- 回归循环的脚本化思路

也就是说，W5 不是另起炉灶，而是：

> 用 W4 已经练顺的“作品组织方式”，切换到更适合 DMA 的设备后端，继续向更深的方向推进。

---

## 8. 为什么这条路线更适合市场表达

因为岗位真正关心的，不是“你是不是只会一个 QEMU 小设备”，而是你能不能把这套话讲完整：

- 我做过 PCI 设备枚举与 BAR 映射
- 我做过中断申请与排障
- 我做过用户态工具与错误码
- 我做过卸载与循环稳定性
- 后续我还能把 DMA、mmap、bench、perf、ftrace 接上

这比直接说“我做过 ivshmem demo”要强很多。

---

## 9. W4 一句话目标

> 用 ivshmem-doorbell 把 PCI 设备发现、BAR/MMIO、消息中断、用户态接口、remove 和证据链归档这套基本功真正学透，为 W5 的 DMA/性能分析闭环打基础。
