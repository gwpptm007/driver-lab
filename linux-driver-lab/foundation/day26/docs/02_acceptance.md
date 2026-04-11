# Day26 验收分析（结合本轮真实输出）

本轮 records 位于：

```text
records/day26-local-001/
```

> 如果你当前目录里还是历史命名（例如 `day25-local-001`），只要里面的内容是本轮 day26 输出，也可以按同样方法判定。

## 最终结论

**Day26 通过。**

它不仅做到了 EDU 可见、驱动 probe 成功，还把“用户态工具触发一次中断 -> 内核 IRQ handler 进入 -> 驱动私有计数增长 -> `/proc/interrupts` 全局计数增长 -> 错误输入返回清晰错误”整条链路跑通了。

## 如何逐项验证

### 1. `run-summary.md`：总摘要先看是否全绿

`run-summary.md` 当前这轮结果是：

- `edu device visible: yes`
- `probe success: yes`
- `ioctl info works: yes`
- `read state works: yes`
- `driver irq_count grows: yes`
- `/proc/interrupts entry exists: yes`
- `/proc/interrupts count grows: yes`
- `invalid trigger error clear: yes`
- `guest flow complete: yes`

这表示 day26 的 9 个核心验收项已经全部成立。

### 2. `lspci-nn.txt`：确认 QEMU EDU 设备已被枚举

关键行是：

```text
00:02.0 Class [00ff]: Device [1234:11e8] (rev 10)
```

这证明：

- QEMU 里确实挂的是 EDU 设备；
- guest 侧 PCI 枚举已经发现它；
- 后续驱动接住的对象就是 `1234:11e8`。

### 3. `lspci-vv-nn.txt`：确认 BAR0 与 MSI 能力存在

这一轮最关键的两段是：

- `Region 0: Memory at 10000000 ... [size=1M]`
- `Capabilities: [40] MSI: Enable- Count=1/1 Maskable- 64bit+`

这说明：

- 驱动后面要用的 BAR0 资源确实存在；
- EDU 设备本身具备 MSI capability；
- day26 驱动里 `pci_alloc_irq_vectors(..., PCI_IRQ_MSI)` 这条路有硬件模型支撑。

### 4. `dmesg-driver.txt`：确认 probe / trigger / irq handler 都真发生了

这一轮最关键的日志是：

- `probe enter: 1234:11e8`
- `BAR0: start=0x10000000 len=0x100000`
- `MSI vector=50`
- `probe success`
- `write trigger: value=0x00000001`
- `irq handler: irq=50 status=0x00000001 count=1`

这几行连在一起说明：

1. 驱动成功匹配到 EDU；
2. BAR0 已经成功映射；
3. MSI 向量已经建立；
4. 用户态 write 触发成功；
5. IRQ handler 确实进入了一次；
6. handler 读到的中断状态是 `0x1`，并把内部计数加到 `1`。

### 5. `info-before.txt`：确认 ioctl `GET_INFO` 链路成立

关键信息包括：

- `tool_api_version=1`
- `vendor=0x1234 device=0x11e8`
- `irq_vector=50 irq_count=0 msi_enabled=1`
- `identity_value=0x010000ed`
- `last_irq_status=0x00000000 last_ack_value=0x00000000`

这说明：

- `/dev/day26_edu0` 已经可用；
- ioctl 读取设备状态的路径通了；
- 在触发前，计数确实是 `0`；
- `msi_enabled=1` 也从用户态视角再次印证了 MSI 已建立。

### 6. `read-state-before.txt` / `read-state-after.txt`：确认 `read()` 文本接口成立

这两个文件内容结构一致，但 after 里最关键的变化是：

- `irq_count=1`
- `last_irq_status=0x00000001`
- `last_ack_value=0x00000001`

说明：

- 驱动实现的 `read()` 文本输出可用；
- 它不是固定死值，而是真实反映中断前后的状态变化；
- ACK 也确实写回了 `0x1`。

### 7. `trigger.txt`：确认用户态 `write()` 触发成功

当前内容包含：

- 驱动日志：`write trigger: value=0x00000001`
- 驱动日志：`irq handler: irq=50 status=0x00000001 count=1`
- 用户态工具输出：`triggered value=0x00000001`

这很关键，因为它把三层证据串起来了：

- 用户态工具真的发出了 write；
- 驱动真的收到了 write；
- 设备真的引发了中断；
- IRQ handler 真的处理了这次中断。

### 8. `irq-count-before.txt` / `irq-count-after.txt`：确认驱动私有计数增长

本轮结果是：

- before：`irq_count=0`
- after：`irq_count=1`

这说明驱动内部计数完成了最基本的 `0 -> 1` 增长。

### 9. `proc-interrupts-before.txt` / `proc-interrupts-after.txt`：确认内核全局中断统计也增长

本轮结果是：

- before：`50:          0       MSI 32768 Edge      day26_edu_tool`
- after：`50:          1       MSI 32768 Edge      day26_edu_tool`

这说明：

- 不只是驱动私有计数涨了；
- Linux 内核的 `/proc/interrupts` 全局统计也确认这次中断真实发生。

### 10. `invalid-trigger-zero.txt`：确认错误输入返回清晰错误

本轮结果是：

- `write trigger failed: Invalid argument`
- `rc=5`

这对应了用户态工具约定的返回码：

- `DAY26_RC_WRITE = 5`

而内核驱动侧 `write()` 明确对 `0` 返回 `-EINVAL`。

这条证据说明：

- 负向路径不是“沉默失败”；
- 用户态能拿到明确错误信息；
- 用户态也能拿到清晰退出码。

### 11. `reset-stats.txt`：确认清零统计的 ioctl 有效

结果是：

- `stats reset`
- `irq_count=0`

说明 `DAY26_IOC_RESET_STATS` 已经生效。

### 12. `serial.log`：确认 guest 自动流程完整跑完

本轮串口日志里能看到：

- `probe success`
- `write trigger`
- `irq handler`
- `remove enter`
- `remove leave`
- `===DAY26:COMPLETE===`

这说明整条自动流程完整走通，并且模块卸载也成功。

### 13. `qemu.stderr.log`：确认 QEMU 无额外错误

当前文件为空，说明这轮没有额外的 QEMU 错误输出。

## Day26 最终通过项汇总

可以把 day26 的通过项总结为：

1. EDU 设备枚举成功；
2. BAR0 成功映射；
3. MSI 成功建立；
4. `ioctl` 信息接口可用；
5. `read()` 状态文本接口可用；
6. `write()` 触发中断路径可用；
7. 驱动内部 `irq_count` 从 `0 -> 1`；
8. `/proc/interrupts` 全局计数从 `0 -> 1`；
9. 错误输入 `trigger 0` 返回清晰错误；
10. `reset-stats` 清零统计成功；
11. `remove` 成功，guest 自动流程完整结束。

## 结论

**Day26 通过。**

而且这次通过不是“只到 probe 成功”，而是已经完成了：

> 用户态工具 -> 驱动 write/ioctl/read -> EDU 设备中断 -> 内核 IRQ handler -> 驱动状态更新 -> `/proc/interrupts` 增长 -> 错误码清晰返回

这一整条闭环。
