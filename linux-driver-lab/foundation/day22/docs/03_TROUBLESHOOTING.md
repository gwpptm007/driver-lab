# 03_TROUBLESHOOTING：day22 常见问题只保留最实用的版本

## 1. `make check` 提示 `CONFIG_PCI is not set`

先修 PCI 配置，再继续 day22。

```bash
source env/local.wq7.env
make kernel-pci-prep
```

然后回到源码目录重新编 `Image`：

```bash
cd "$KERNEL_SRC_ROOT"
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
unset CC CXX LD AR AS NM STRIP OBJCOPY OBJDUMP READELF
make -C "$KERNEL_SRC_ROOT" -j"$(nproc)" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- O="$KDIR" Image
```

---

## 2. `scripts/config` 找不到

你当前这套内核树源码根目录是：

```bash
/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10/src
```

不是 `linux-5.15.10/` 上一级。

---

## 3. 重编 `Image` 时出现 `gcc: ... -mlittle-endian`

说明你实际用了宿主机 `gcc`，不是 `aarch64-linux-gnu-gcc`。

先固定：

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
unset CC CXX LD AR AS NM STRIP OBJCOPY OBJDUMP READELF
```

---

## 4. `make selftest-tool` 报 `Exec format error`

根因是：guest 工具是 arm64，而宿主机是 x86_64。

当前版本已经修成：
- guest 保留 arm64 二进制；
- 自测时自动临时编 host 版工具。

如果这里再失败，优先看：

```bash
ls -l workdir/tools/host/pci_sysfs_dump
cat workdir/selftest-pci-sysfs.out
```

---

## 5. `make rootfs` 报 `mknod ... Operation not permitted`

直接用：

```bash
source env/local.wq7.env
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
```

同理，如果 `make run` 内部再次卡在这里，也直接：

```bash
sudo -E make run
```

---

## 6. `git clone https://github.com/pciutils/pciutils.git` 失败，提示 `Could not resolve host`

这不是 day22 代码问题，是宿主机 DNS/网络问题。

优先处理顺序：
1. 先复用本机已有的 `pciutils/lspci`；
2. 再考虑离线拷贝源码；
3. 最后才是修网络。

---

## 7. `make module` 失败，`modpost` 报 `pci_unregister_driver` / `__pci_register_driver` undefined

当前建议：
- 不用它否定 day22；
- 作为 day23 的前置问题单独收口。

因为 day22 的核心目标是：
- 设备可枚举；
- `lspci -nn / -vv -nn` 有证据；
- guest 自动流程跑完。

---

## 8. `run-summary.md` 写“否”，但实际串口日志已经成功

当前版本里，`run-summary.md` 可能误判。

最终结论请优先看：

```bash
sed -n '1,220p' records/${RUN_ID}/lspci-vv-nn.txt
grep 'DAY22' records/${RUN_ID}/serial.log
```

尤其关注：
- `1af4:1110`
- `Region 0`
- `Region 2`
- `===DAY22:COMPLETE===`
