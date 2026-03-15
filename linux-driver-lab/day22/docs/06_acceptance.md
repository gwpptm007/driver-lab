# day22 验收标准

## 必须满足

- [ ] guest 内 `lspci -nn` 能看到目标设备
- [ ] `lspci -vv -nn` 已归档
- [ ] `dmesg` 中有 PCI 枚举相关日志
- [ ] `/sys/bus/pci/devices` 已归档
- [ ] `qemu-command.txt` 已归档
- [ ] `run-summary.md` 已生成

## 最小通过判定

如果下面三条同时满足，就可以认为 day22 通过：

1. `lspci-nn.txt` 中出现 `1af4:1110`
2. `lspci-vv-nn.txt` 非空
3. `sysfs-pci-devices.txt` 非空

## 建议额外补充

- [ ] `kernel-config-check.txt` 已保存
- [ ] 失败样例也有单独记录
- [ ] QEMU 启动参数被复制到 README 或最终报告中
