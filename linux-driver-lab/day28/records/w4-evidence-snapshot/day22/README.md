# records 目录说明

day22 最终验收最关键的证据都在这里。

推荐优先级：

1. `serial.log`
2. `lspci-nn.txt`
3. `lspci-vv-nn.txt`
4. `dmesg-pci.txt`
5. `sysfs-pci-devices.txt`
6. `qemu.stderr.log`

## 当前判定口径

当前版本里，`run-summary.md` 仍可能误判。

所以 day22 最终请优先以：
- `serial.log` 中的 marker；
- `lspci` 实际输出；
- PCI BAR 信息；

来判断是否通过。

## 最关键的成功标记

- `00:02.0 Class [0500]: Device [1af4:1110] (rev 01)`
- `Region 0: Memory at ... [size=256]`
- `Region 2: Memory at ... [size=4M]`
- `===DAY22:COMPLETE===`
