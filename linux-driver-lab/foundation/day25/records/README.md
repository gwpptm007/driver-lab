# records 说明

`records/<RUN_ID>/` 用来保存一次完整 day25 运行的证据。

当前上传的 `records/day25-local-001/` 已经形成完整闭环：
- EDU 设备枚举成功
- 驱动 probe 成功，MSI vector 已建立
- 用户态工具成功打开 `/dev/day25_edu0`
- 触发 EDU 中断成功
- 驱动内部 `irq_count` 从 `0 -> 1`
- `/proc/interrupts` 中 `day25_edu_irq` 也从 `0 -> 1`
- `remove` 成功，guest 流程跑到 `===DAY25:COMPLETE===`

复核这轮结果时，重点看这些文件：
- `lspci-nn.txt`
- `lspci-vv-nn.txt`
- `irq-info-before.txt`
- `trigger.txt`
- `irq-count-before.txt`
- `irq-count-after.txt`
- `proc-interrupts-before.txt`
- `proc-interrupts-after.txt`
- `dmesg-driver.txt`
- `serial.log`
