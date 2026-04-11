# day23 records 说明

最终版建议至少归档以下文件：

- `run-summary.md`
- `serial.log`
- `dmesg-probe.txt`
- `dmesg-remove.txt`
- `lspci-vv-nn.txt`
- `qemu.stderr.log`

其中最重要的是：

- `run-summary.md`：快速判断 6 项核心结果
- `serial.log`：完整事实来源
- `dmesg-probe.txt`：证明 `probe + BAR`
- `dmesg-remove.txt`：证明 `remove`

最终验收关键字符串：

- `probe enter`
- `BAR0:`
- `BAR2:`
- `BAR0 mapped`
- `BAR2 mapped`
- `probe success`
- `remove enter`
- `remove leave`
- `===DAY23:COMPLETE===`
