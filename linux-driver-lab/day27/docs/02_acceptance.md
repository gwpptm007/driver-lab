# Day27 验收说明

## 1. 为什么 Day27 单独存在

前几天已经证明：
- 设备可见
- `pci_driver` 能接住设备
- 用户态工具 / 中断能打通

Day27 的重点转成：
> 当驱动被重复装卸 200 次时，资源释放是否对称，系统是否仍然稳定。

## 2. 关键证据文件与如何解读

### 2.1 `lspci-nn.txt`
当前记录内容里能看到：
- `00:02.0 Class [00ff]: Device [1234:11e8] (rev 10)`

这说明 QEMU EDU 设备已经被 guest 成功枚举出来。

### 2.2 `lspci-vv-nn.txt`
当前记录里能看到：
- `Region 0: Memory at 10000000 ... [size=1M]`
- `Capabilities: [40] MSI: Enable- Count=1/1 Maskable- 64bit+`

这两点说明：
1. BAR0 存在，驱动后续有可映射的 MMIO 空间；
2. 设备模型本身具备 MSI capability，Day27 的 IRQ 测试是有硬件/模型基础的。

### 2.3 `loop-summary.txt`
当前记录是：
- `loop_count=200`
- `pass=200`
- `fail=0`

这说明 guest 中的 200 次循环全部完成，没有任何一轮在 `insmod / smoke / rmmod` 中途失败。

### 2.4 `dmesg-driver.txt`
从当前记录可以看到反复出现：
- `probe enter: 1234:11e8`
- `BAR0: start=0x10000000 len=0x100000 flags=0x40200`
- `MSI vector=50 enabled=1`
- `probe success`
- `write trigger: value=0x00000001`
- `irq handler: irq=50 status=0x00000001 count=1`
- `remove enter`
- `remove leave`

这些日志共同证明：
1. 每轮都能成功 probe 到 EDU 设备；
2. 每轮都能建立 MSI 中断；
3. 每轮 smoke 都真实触发了一次中断，而不是空跑；
4. remove 路径每轮都能完整走完。

### 2.5 `serial.log`
`serial.log` 是最强证据，因为它保留了 guest 内完整串口输出。
当前记录中：
- 大量出现 `probe success / irq handler / remove leave`；
- 最末尾出现 `===DAY27:COMPLETE===`；
- 没有出现 `BUG:`、`Oops:`、`Kernel panic`、`hung task` 等异常关键字。

这说明 guest 自动流程完整跑完，而且整个 200 次循环过程中没有出现内核级稳定性故障。

### 2.6 `proc-interrupts-final.txt`
这个文件是补充证据，用来说明循环全部结束后，guest 中 `/proc/interrupts` 仍然可读、系统仍处于可观测状态。

## 3. 关于 `run-summary.md` 的一个已知假阴性

当前上传记录里的 `run-summary.md` 中：
- `loop target met (200): no`

但同一批记录中的 `loop-summary.txt` 已经明确给出：
- `pass=200`
- `fail=0`

同时 `serial.log` 也完整跑到了 `===DAY27:COMPLETE===`。

因此这不是测试失败，而是 `scripts/08_extract_records.sh` 解析 `loop-summary.txt` 时未去掉回车字符，导致比较 `200` 与 `200` 时出现假阴性。

新版脚本已经修正为：
- 在读取 `pass=` / `fail=` 时去掉 `` 和空白字符；
- 重新生成 `run-summary.md` 时，`loop target met (200)` 会正确写成 `yes`。

## 4. 最终通过标准

Day27 判通过，需要同时满足：
1. EDU 设备枚举成功；
2. 200 次循环全部通过；
3. `probe/remove` 日志完整出现；
4. IRQ handler 日志出现，说明每轮 smoke 并非空跑；
5. 无 `BUG:` / `Oops:` / `Kernel panic` / `hung task`；
6. guest 自动流程最终跑到 `===DAY27:COMPLETE===`。

## 5. 结合当前测试结果的最终结论

结合当前上传的 `records/day27-local-001/`：
- EDU 枚举成功；
- 200 次循环全部通过；
- 每轮都能 `probe success -> trigger irq -> irq handler -> remove leave`；
- 无内核异常；
- guest 完整结束。

**因此，Day27 通过。**
