# day23 排障（最终版）

## 1. `make module` 报 x86 相关编译参数

现象：

- `-mcmodel=kernel`
- `-mno-sse`
- `-m64`

说明外部模块构建走成了 x86 规则，但编译器却是 `aarch64-linux-gnu-gcc`。

处理：

- 确认 `source env/local.wq7.env`
- 确认 `ARCH=arm64`
- 确认 `CROSS_COMPILE=aarch64-linux-gnu-`
- 用新版脚本执行 `make module`

## 2. `modpost` 提示 PCI 符号 undefined

常见现象：

- `__pci_register_driver undefined`
- `pci_unregister_driver undefined`
- `pci_enable_device undefined`
- `pci_iomap undefined`

这通常不是 day23 C 代码问题，而是当前 arm64 build tree 的 `Module.symvers` 没跟新的 PCI 配置同步。

处理：

```bash
make kernel-module-tree
make module
```

如果 `kernel-module-tree` 仍不够，就回到内核源码目录显式执行：

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
unset CC CXX LD AR AS NM STRIP OBJCOPY OBJDUMP READELF

make -C /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src   ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-   O=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64   modules_prepare

make -C /home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src   -j"$(nproc)" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-   O=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/build/arm64   modules
```

## 3. `make rootfs` 报 `mknod: Operation not permitted`

说明当前用户没有 `mknod` 权限。

处理：

```bash
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
```

## 4. `make run` 没有 COMPLETE marker

优先看：

```bash
cat records/${RUN_ID}/qemu.stderr.log
tail -n 200 records/${RUN_ID}/serial.log
```

## 5. `lspci` 没准备好

如果本机不能联网，直接从别处拷 `pciutils` 源码到：

```text
 day23/third_party/pciutils
```

然后执行：

```bash
make build-lspci
```

## 6. `qemu.stderr.log` 有 `share` 参数弃用警告

这是 QEMU 对短格式布尔参数的提示，不影响 day23 功能正确性。后续有时间把相关参数改成 `share=on` 即可。
