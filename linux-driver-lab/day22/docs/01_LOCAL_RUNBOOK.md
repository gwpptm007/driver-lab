# 01_LOCAL_RUNBOOK：拿到代码后在本地机器上跑通 day22 的全流程

这份 runbook 只解决一个问题：

> 拿到 day22 代码后，在本地机器上从 0 跑到可验收状态，应该按什么顺序执行。

---

## 1. 进入目录并载入环境

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day22
source env/local.wq7.env
```

先确认变量已经生效：

```bash
echo "$KERNEL_SRC_ROOT"
echo "$KDIR"
echo "$KERNEL_IMAGE"
echo "$KERNEL_CONFIG_PATH"
echo "$BUSYBOX_BIN"
echo "$PCIUTILS_SRC_DIR"
echo "$GUEST_LSPCI_BIN"
```

如果没有 `env/local.wq7.env`，先从模板复制：

```bash
cp env/local.example.env env/local.wq7.env
vi env/local.wq7.env
source env/local.wq7.env
```

---

## 2. 检查宿主机环境与内核配置

```bash
make check
```

这一步至少要确认：

- `qemu-system-aarch64` 可用；
- `KERNEL_IMAGE / BUSYBOX_BIN / GUEST_LSPCI_BIN` 可用；
- 内核配置里有：
  - `CONFIG_PCI=y`
  - `CONFIG_PCI_MSI=y`
  - `CONFIG_PCI_HOST_GENERIC=y`

如果这里不过，先看 `docs/03_TROUBLESHOOTING.md`。

---

## 3. 编 guest 侧工具

```bash
make build-tools
```

检查：

```bash
ls -l workdir/tools/aarch64/pci_sysfs_dump
file workdir/tools/aarch64/pci_sysfs_dump
```

预期：
- 文件存在；
- 可执行；
- 架构是 `ARM aarch64`。

---

## 4. 做宿主机自测

```bash
make selftest-tool
```

检查：

```bash
ls -l workdir/tools/host/pci_sysfs_dump
cat workdir/selftest-pci-sysfs.out
```

说明：
- `build-tools` 生成的是 guest 用的 arm64 二进制；
- `selftest-tool` 会临时编一个 host 版工具，在 x86_64 宿主机上做伪 sysfs 自测。

---

## 5. 构建独立 initramfs

这一步通常需要 `mknod` 创建设备节点，所以建议直接用：

```bash
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
```

检查：

```bash
ls -l workdir/rootfs.img
find workdir/rootfs -maxdepth 2 -type f | sort
```

至少应看到：
- `workdir/rootfs/init`
- `workdir/rootfs/bin/busybox`
- `workdir/rootfs/bin/lspci`
- `workdir/rootfs/bin/pci_sysfs_dump`
- `workdir/rootfs.img`

---

## 6. 准备 ivshmem backend

```bash
make backend
```

检查：

```bash
ls -l workdir/runs/${RUN_ID}/ivshmem-day22.bin
```

---

## 7. 正式执行 day22

```bash
make run
```

它会完成：
- QEMU 启动 arm64 guest；
- guest 内自动执行 `lspci -nn`；
- guest 内自动执行 `lspci -vv -nn`；
- guest 内抓取 PCI 相关 `dmesg`；
- 归档 `records/${RUN_ID}/...`。

如果 `make run` 卡在 `mknod ... Operation not permitted`，请直接：

```bash
sudo -E make run
```

---

## 8. 如何看 day22 是否通过

先看：

```bash
cat records/${RUN_ID}/run-summary.md
```

再看真实证据：

```bash
sed -n '1,120p' records/${RUN_ID}/lspci-nn.txt
sed -n '1,220p' records/${RUN_ID}/lspci-vv-nn.txt
sed -n '1,220p' records/${RUN_ID}/dmesg-pci.txt
grep 'DAY22' records/${RUN_ID}/serial.log
```

### 当前推荐的最终判定口径

不要只看 `run-summary.md`。

当前版本里，`run-summary.md` 仍可能存在误判；请优先以 `serial.log` 的 marker 和 `lspci` 实际输出为准。详细标准见：

- `docs/02_RESULTS_AND_ACCEPTANCE.md`

---

## 9. `make module` 怎么处理

```bash
make module
```

当前这一步不是 day22 的核心验收门槛。若失败，优先继续完成 day22 的设备可见性验证，再把模块构建问题带到 day23 处理。
