# day28：W4 结果与验收

## 1. W4 到底完成了什么

W4 的原始目标可以理解为一条逐层递进的 PCIe 学习闭环：

1. **day22**：确认 QEMU PCI 设备可见，完成 `lspci -vv` 证据归档
2. **day23**：写出 `pci_driver` 骨架，完成 `probe/remove + BAR` 验证
3. **day24**：完成 MMIO/共享内存窗口读写闭环
4. **day25**：完成 MSI 中断触发与计数增长闭环
5. **day26**：完成用户态工具与清晰错误码闭环
6. **day27**：完成 200 次循环装卸的稳定性验证

从当前上传仓库中的真实 `records/` 看，这条链已经完整建立起来。

---

## 2. 各天通过项怎么判断

### day22：设备可见性通过

虽然 `day22/records/day22-local-001/run-summary.md` 写的是“否”，但原始串口日志已经明确给出：

- `pci 0000:00:02.0: [1af4:1110]`
- `00:02.0 Class [0500]: Device [1af4:1110]`
- `===DAY22:LSPCI_VV_NN:BEGIN===`
- `===DAY22:DMESG_PCI:BEGIN===`
- `===DAY22:COMPLETE===`

这说明：

- PCI 总线正常起来了
- `ivshmem` 设备被成功枚举
- `lspci -vv` 和 PCI dmesg 都跑出来了
- guest 自动流程完整结束

所以 day22 的真实结论是：

> **核心通过，旧版 `run-summary.md` 为误判。**

### day23：`pci_driver` 骨架通过

`day23/records/day23-local-001/run-summary.md` 已明确给出：

- `insmod 成功：yes`
- `probe 成功：yes`
- `BAR0 信息：yes`
- `BAR2 信息：yes`
- `rmmod 成功：yes`
- `guest 流程完成：yes`

这说明 day23 已经把“设备可见”推进到了“驱动接住设备”。

### day24：MMIO 闭环通过

`day24/records/day24-local-001/run-summary.md` 已给出：

- `mmio info：yes`
- `mmio write：yes`
- `mmio read after：yes`
- `shm write：yes`
- `shm read：yes`

这说明：

- BAR0 安全读验证成立
- BAR2 共享内存协议头与 payload 闭环成立

### day25：MSI 中断闭环通过

`day25/records/day25-local-001/run-summary.md` 已给出：

- `driver irq_count grows: yes`
- `/proc/interrupts entry exists: yes`
- `/proc/interrupts count grows: yes`

这说明：

- 用户态触发动作成功
- IRQ handler 真进入了
- 驱动内部计数和内核全局中断统计都增长了

### day26：用户态工具闭环通过

`day26/records/day26-local-001/run-summary.md` 已给出：

- `ioctl info works: yes`
- `read state works: yes`
- `driver irq_count grows: yes`
- `invalid trigger error clear: yes`

这说明 day26 已经把接口做成了更像一个“可用小工具”，而不是只靠 dmesg 观察。

### day27：200 次循环稳定性通过

`day27/records/day27-local-001/loop-summary.txt` 与 `run-summary.md` 已明确给出：

- `loop pass: 200`
- `loop fail: 0`
- `loop target met (200): yes`
- `oops/hung/panic found: no`

这说明 day27 不只是“能跑一次”，而是：

> **在 200 次装卸循环里都保持稳定，没有出现明显内核崩溃类问题。**

---

## 3. W4 最终验收结论

综合 day22~day27，W4 的最终结论可以写成：

> **W4 通过。**
>
> 当前上传仓库中的真实 records 已经证明：
> - PCI 设备可见
> - `pci_driver` 骨架可用
> - MMIO 读写闭环可用
> - MSI 中断闭环可用
> - 用户态工具接口可用
> - 200 次循环稳定性通过

---

## 4. W4 的已知限制

day28 要写清楚“通过”和“完美”不是一回事。

当前 W4 仍然有这些边界：

- 主要验证环境仍是 QEMU 教学设备（ivshmem、EDU）
- 还没有进入真实 DMA 能力验证
- 还没有进入 mmap/bench/perf/ftrace 的 W5 主题
- day22 的旧版 `run-summary.md` 曾出现误判，已经通过 day28 的汇总逻辑修正说明

---

## 5. W5 的输入是什么

W5 的起点已经具备：

- 稳定的 PCIe/QEMU 复现实验环境
- 清晰的 driver + tool + guest + records 结构
- 明确的中断、状态、证据链采集方式

因此下一阶段可以自然进入：

- DMA coherent
- mmap
- bench
- perf
- ftrace
