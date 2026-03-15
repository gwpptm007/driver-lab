# day22 验收清单（实做版）

## 必须满足

- [ ] `make check` 能通过主机环境检查
- [ ] `make rootfs` 成功生成 `workdir/rootfs.img`
- [ ] `make run` 能产出 `records/<run-id>/`
- [ ] `lspci-nn.txt` 中出现 `1af4:1110`
- [ ] `lspci-vv-nn.txt` 非空
- [ ] `sysfs-pci-devices.txt` 非空
- [ ] `run-summary.md` 已生成

## 推荐额外检查

- [ ] `kernel-config-check.txt` 已保留
- [ ] `serial.log` 已保留
- [ ] `qemu-command.txt` 已保留
- [ ] `server.log` 已保留

## 不通过时优先排查

1. `GUEST_LSPCI_BIN` 是否真的是 arm64 二进制
2. `ivshmem-server` 是否成功建立 socket
3. `KERNEL_CONFIG_PATH` 中 PCI / MSI 是否打开
4. `serial.log` 是否出现 `===DAY22:COMPLETE===?`
