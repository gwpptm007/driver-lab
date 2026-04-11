# Day27 records 说明

`records/<RUN_ID>/` 中的关键文件：

- `run-summary.md`：最终摘要
- `lspci-nn.txt`：EDU 设备可见性证据
- `lspci-vv-nn.txt`：BAR / MSI capability 证据
- `loop-summary.txt`：200 次循环统计，当前是否通过首先看这里
- `proc-interrupts-final.txt`：最终中断条目
- `dmesg-driver.txt`：驱动 probe/remove/irq 关键日志
- `serial.log`：完整串口日志，最终以它为准
- `qemu.stderr.log`：QEMU stderr

## 结合当前上传记录如何判通过

当前 `records/day27-local-001/` 的判断顺序建议是：
1. 看 `lspci-nn.txt` 是否有 `1234:11e8`；
2. 看 `loop-summary.txt` 是否为 `loop_count=200 / pass=200 / fail=0`；
3. 看 `dmesg-driver.txt` 是否反复出现 `probe success / irq handler / remove leave`；
4. 看 `serial.log` 末尾是否有 `===DAY27:COMPLETE===`；
5. 确认 `qemu.stderr.log` 为空或只有无害告警。
