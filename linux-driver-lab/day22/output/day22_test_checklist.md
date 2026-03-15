# day22 测试检查表

## 环境

- [ ] 已准备 arm64 `Image`
- [ ] 已准备 arm64 BusyBox
- [ ] 已准备 arm64 `lspci`
- [ ] `qemu-system-aarch64` 可用
- [ ] `ivshmem-server` 可用

## 执行

- [ ] `make check` 通过
- [ ] `make rootfs` 成功生成 `workdir/rootfs.img`
- [ ] `make run` 成功执行完成

## 结果

- [ ] `records/<run-id>/lspci-nn.txt` 中存在 `1af4:1110`
- [ ] `records/<run-id>/lspci-vv-nn.txt` 非空
- [ ] `records/<run-id>/sysfs-pci-devices.txt` 非空
- [ ] `records/<run-id>/serial.log` 已保留
- [ ] `records/<run-id>/run-summary.md` 已生成
