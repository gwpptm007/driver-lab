# day24 故障排查

## 独立构建约束

- day24 只使用“当前 day 自己准备”的 `third_party/pciutils/`
- 不复用其它 day 目录里的 `lspci` 或中间产物
- 第三方源码不随包提供；请按主流程先获取再编译

## 1. `make module` 报 PCI 符号 undefined

现象通常是：

- `__pci_register_driver undefined`
- `pci_unregister_driver undefined`
- `pci_enable_device undefined`
- `pci_iomap undefined`

处理：

```bash
make kernel-module-tree
make module
```

如果仍然失败，就回到内核源码目录显式执行 `modules_prepare + modules`。

## 2. `make rootfs` 报 `mknod: Operation not permitted`

说明当前用户没有 `mknod` 权限。

处理：

```bash
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
```

## 3. `build-lspci` 失败

如果本机不能联网，就把你自行获取的 `pciutils` 源码离线拷到：

```text
day24/third_party/pciutils
```

然后再执行：

```bash
make build-lspci
```

如果出现：

```text
./configure: Permission denied
```

先执行：

```bash
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

## 4. `make run` 没有 COMPLETE marker

优先看：

```bash
cat records/${RUN_ID}/qemu.stderr.log
tail -n 200 records/${RUN_ID}/serial.log
```

## 5. `mmio-write` 报 offset not allowed

这是 day24 的刻意限制。

为了不乱写 ivshmem BAR0 寄存器，day24 只允许通过 `mmio-write` 改协议头中的安全字段：

- `seq`
- `state`
- `payload_len`

如果要改 payload，使用：

```bash
day24_mmio_tool shm-write <text>
```

## 6. `shm-read` 读不回预期字符串

先检查：

- `mmio-info` 中 `payload_len` 是否正常
- `dmesg-driver.txt` 里是否有 `payload write` 日志
- guest 工具是否真的执行到了 `shm-write`

## 7. 结果已经跑出来，但不知道算不算通过

请不要只盯 `run-summary.md`，要同时看：

- `mmio-info.txt`
- `mmio-read-before.txt`
- `mmio-write-state.txt`
- `mmio-read-after.txt`
- `shm-write.txt`
- `shm-read.txt`
- `dmesg-driver.txt`
- `lspci-vv-nn.txt`

这些文件怎么解释，见 `docs/02_RESULTS_AND_ACCEPTANCE.md`。
