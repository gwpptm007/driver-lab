# Day25 结果与验收说明

## 最终结论

基于当前上传的 `records/day25-local-001/`，**day25 已完整通过**。

这轮结果已经同时证明：

1. EDU PCI 设备成功枚举；
2. day25 驱动成功接管设备并申请 MSI vector；
3. 用户态工具成功打开 `/dev/day25_edu0`；
4. 用户态向 EDU 触发寄存器写入后，IRQ handler 真实进入；
5. 驱动内部 `irq_count` 从 `0` 增长到 `1`；
6. `/proc/interrupts` 中 `day25_edu_irq` 的计数也从 `0` 增长到 `1`；
7. 模块 `remove` 成功，guest 自动流程跑到 `===DAY25:COMPLETE===`。

因此，day25 的最终判定是：

> **通过：EDU 设备枚举 + MSI 建立 + 用户态触发中断 + 驱动计数增长 + /proc/interrupts 增长 + remove 完整结束。**

## 如何通过每个输出文件判断结果

### 1. `run-summary.md`
这是最短摘要。当前上传这份已经是：
- `edu device visible: yes`
- `probe success: yes`
- `BAR0 logged: yes`
- `driver irq_count grows: yes`
- `/proc/interrupts entry exists: yes`
- `guest flow complete: yes`

注意：当前脚本摘要项里写的是 `entry exists`，但结合下面 `proc-interrupts-before/after.txt`，实际已经能证明“不只是 entry 存在，而是计数也增长了”。

### 2. `lspci-nn.txt`
这里看到：
- `00:02.0 Class [00ff]: Device [1234:11e8] (rev 10)`

这证明 QEMU EDU 设备已经成功出现在 guest 的 PCI 总线上。

### 3. `lspci-vv-nn.txt`
这份文件提供两个关键证据：

- `Region 0: Memory at 10000000 ... [size=1M]`
- `Capabilities: [40] MSI: Enable- Count=1/1 Maskable- 64bit+`

这说明：
- BAR0 已被成功识别；
- 设备本身确实暴露了 MSI capability；
- day25 选择 EDU 做 MSI 实验是成立的。

### 4. `dmesg-driver.txt`
这是最重要的内核态证据。当前文件已经明确显示：

- `probe enter: 1234:11e8`
- `enabling device`
- `BAR0: start=0x10000000 len=0x100000 flags=...`
- `MSI vector=50 ident=0x010000ed liveness=0xa5a55a5a inverted=0x5a5aa5a5`
- `probe success`
- `trigger irq: value=0x00000001`
- `irq handler: irq=50 status=0x00000001 count=1`

这些日志合起来证明：
- 驱动已经接住 EDU；
- `pci_enable_device / pci_request_regions / pci_iomap / pci_alloc_irq_vectors / request_irq` 已全部成功；
- 用户态触发后，内核 IRQ handler 确实真实进入，并把 `irq_count` 加到了 1。

### 5. `irq-info-before.txt`
这份文件证明用户态工具能正常打开 `/dev/day25_edu0`，并读取静态信息。当前内容里能看到：

- `vendor=0x1234 device=0x11e8`
- `bar0_start=0x10000000 bar0_len=0x100000`
- `irq_vector=50 irq_count=0 msi_enabled=1`
- `last_irq_status=0x00000000 ack_value=0x00000000`
- `liveness_value=0xa5a55a5a liveness_inverted=0x5a5aa5a5`

这证明：
- `/dev/day25_edu0` 已经可用；
- ioctl 读取链路正常；
- probe 阶段保存下来的关键状态都能从用户态读出来。

### 6. `trigger.txt`
这里有：
- `triggered value=0x00000001`

这说明用户态工具已经成功调用了 `DAY25_IOC_TRIGGER_IRQ`，驱动也已经把值写进 EDU 的 IRQ 触发寄存器。

### 7. `irq-count-before.txt` 与 `irq-count-after.txt`
这是最直接的第一条“中断闭环”证据：

- before: `irq_count=0`
- after:  `irq_count=1`

这说明从驱动自己的视角看，中断次数已经真实增长，IRQ handler 至少进入了一次。

### 8. `irq-status-before.txt` 与 `irq-status-after.txt`
这里能看到：
- before: `irq_status=0x00000000 ack_value=0x00000000`
- after:  `irq_status=0x00000001 ack_value=0x00000001`

这证明：
- EDU 的中断状态寄存器被成功读到；
- 驱动在 handler 中执行 ACK 的值也是 `0x1`；
- “触发 -> 进入 handler -> 读 status -> 写 ACK” 这条链条是完整的。

### 9. `proc-interrupts-before.txt` 与 `proc-interrupts-after.txt`
这是第二条、且更偏内核全局视角的证据链。

当前文件中：
- before: `50: 0 MSI 32768 Edge day25_edu_irq`
- after:  `50: 1 MSI 32768 Edge day25_edu_irq`

这说明：
- 注册到 `/proc/interrupts` 的 IRQ 条目确实是 `day25_edu_irq`；
- 不是只有驱动私有计数增长，内核全局中断统计也确实增长了。

### 10. `serial.log`
这是总证据。当前串口日志中同时包含：
- `LSPCI_NN`
- `LSPCI_VV_NN`
- `IRQ_INFO_BEFORE`
- `IRQ_COUNT_BEFORE`
- `TRIGGER`
- `IRQ_COUNT_AFTER`
- `PROC_INTERRUPTS_BEFORE/AFTER`
- `DMESG_DRIVER`
- `remove enter`
- `remove leave`
- `===DAY25:COMPLETE===`

这说明：
- guest 自动流程不是中途异常退出；
- 而是完整地按 day25 设计顺序全部执行完了。

## day25 最终验收表

| 验收项 | 结果 | 证据 |
|---|---|---|
| EDU 设备枚举成功 | 通过 | `lspci-nn.txt` 中 `1234:11e8` |
| BAR0 信息可见 | 通过 | `lspci-vv-nn.txt` 与 `dmesg-driver.txt` |
| MSI vector 建立成功 | 通过 | `dmesg-driver.txt` 中 `MSI vector=50` |
| 用户态工具成功打开设备 | 通过 | `irq-info-before.txt` 非报错，且有完整字段 |
| 用户态成功触发中断 | 通过 | `trigger.txt` 中 `triggered value=0x00000001` |
| 驱动内部 irq_count 增长 | 通过 | `irq-count-before.txt` `0 -> 1` |
| `/proc/interrupts` 计数增长 | 通过 | `proc-interrupts-before/after.txt` 中 IRQ 50 `0 -> 1` |
| IRQ handler 真实进入 | 通过 | `dmesg-driver.txt` 中 `irq handler: irq=50 status=0x1 count=1` |
| remove 成功 | 通过 | `serial.log` 中 `remove enter/remove leave` |
| guest 流程完整结束 | 通过 | `serial.log` 中 `===DAY25:COMPLETE===` |


## 最短验证步骤

如果只想快速判定这一轮是否通过，直接执行：

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

判定标准：

- `run-summary.md` 中 7 项都为 `yes`
- `irq-count-before.txt` / `irq-count-after.txt` 为 `0 -> 1`
- `proc-interrupts-before.txt` / `proc-interrupts-after.txt` 中 `day25_edu_irq` 为 `0 -> 1`
- `serial.log` 中同时出现 `probe success`、`irq handler`、`remove enter`、`remove leave`、`===DAY25:COMPLETE===`
