# Day25 - EDU MSI 中断实验（最终版）

## 目标

Day25 的目标是把 **PCI 驱动 + MSI 中断 + 用户态触发** 三条线接起来：

1. 在 QEMU `virt` 平台下挂载 EDU PCI 教学设备；
2. 编写最小 `pci_driver`，完成 `pci_enable_device / pci_request_regions / pci_iomap / pci_alloc_irq_vectors / request_irq`；
3. 通过用户态工具触发一次中断；
4. 从 **驱动日志、驱动内部计数、/proc/interrupts** 三条证据链验证结果；
5. 最后完整执行 `remove`，保证模块可卸载。

## 当前上传 records 的最终结论

基于 `records/day25-local-001/` 这轮真实输出，可以确认 **day25 已完整通过**：

- EDU 设备已经被枚举到：`1234:11e8`
- 驱动 `probe()` 已成功进入并申请到 MSI vector
- BAR0 信息、identity/liveness 检查已经通过
- 用户态工具已经成功打开 `/dev/day25_edu0`
- 用户态向 EDU IRQ_RAISE 写入 `0x1` 后，IRQ handler 成功进入
- 驱动内部 `irq_count` 从 `0 -> 1` 增长
- `/proc/interrupts` 中 `day25_edu_irq` 这一行从 `0 -> 1` 增长
- `remove()` 正常执行，guest 流程已跑到 `===DAY25:COMPLETE===`

因此，day25 的最终结论是：

> **Day25 通过：EDU 设备枚举、MSI 建立、用户态触发中断、驱动内部计数增长、/proc/interrupts 增长、模块卸载 全部闭环完成。**


## 如何快速验证这轮是否真正通过

在 `day25` 目录执行：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day25
source env/local.wq7.env

cat records/${RUN_ID}/run-summary.md
cat records/${RUN_ID}/irq-count-before.txt
cat records/${RUN_ID}/irq-count-after.txt
cat records/${RUN_ID}/proc-interrupts-before.txt
cat records/${RUN_ID}/proc-interrupts-after.txt
grep -n 'probe success\|irq handler\|remove enter\|remove leave\|DAY25:COMPLETE' records/${RUN_ID}/serial.log
```

这轮真实 records 的关键结论是：

- `irq_count` 从 `0 -> 1`
- `/proc/interrupts` 中 `day25_edu_irq` 从 `0 -> 1`
- `serial.log` 中同时出现 `probe success`、`irq handler`、`remove enter`、`remove leave`、`===DAY25:COMPLETE===`

因此，这轮 records 已经足以证明：
**EDU 枚举成功、MSI 建立成功、用户态触发成功、中断处理函数真实进入、驱动和内核全局计数都增长、模块卸载成功。**

## 入口

- `START_HERE.md`：最快入口
- `docs/01_LOCAL_RUNBOOK.md`：完整本地执行流程
- `docs/02_RESULTS_AND_ACCEPTANCE.md`：结合真实输出解释如何判断通过
- `docs/03_TROUBLESHOOTING.md`：常见问题与定位方法
- `output/day25_quick_commands.md`：最短命令清单
