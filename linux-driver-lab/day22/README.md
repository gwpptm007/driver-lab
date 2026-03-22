# day22 最终版：QEMU `ivshmem-plain` PCI 设备可见性验证

day22 的目标只有一个：

> 在本地开发机上启动 arm64 QEMU guest，确认 `ivshmem-plain` 作为 PCI 设备被成功枚举，并把 `lspci / dmesg / 串口日志` 留成可复核证据。

这版 day22 不再把重点放在大量过程性文档，而是只保留最终交付需要的最小内容。

## 先看什么

1. `START_HERE.md`
2. `docs/01_LOCAL_RUNBOOK.md`
3. `docs/02_RESULTS_AND_ACCEPTANCE.md`

## 建议的固定入口

不要每次手敲很多 `export`。先准备本地环境文件，然后每次开新 shell 都先 `source`。

- 模板：`env/local.example.env`
- 结合当前机器路径的示例：`env/local.wq7.env`

推荐流程：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day22
source env/local.wq7.env

make check
make build-tools
make selftest-tool
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
make run
```

## 当前最终口径

### day22 核心是否通过

**核心通过。**

原因：真实运行日志已经证明：
- PCI host bridge 初始化成功；
- `ivshmem` 设备 `1af4:1110` 已被枚举；
- `lspci -vv -nn` 已打印 BAR 信息；
- guest 自动流程已经跑到 `===DAY22:COMPLETE===`。

### 当前仍未完全收口的点

这些问题不影响 day22 的“设备可见性验证”主目标，但需要在收尾时注明：

1. `run-summary.md` 当前存在误判，不能单独作为最终结论来源；
2. guest 内 `pci_sysfs_dump` 当前执行失败；
3. `/init` 里对 `head` 的依赖导致 PCI config 样本没有完整输出；
4. `make module` 当前失败，建议作为 day23 前置问题单独处理，不用于否定 day22。

## day22 里最关键的代码

- `tools/pci_sysfs_dump.c`：guest 观察 `/sys/bus/pci/devices`
- `guest/init.day22`：guest 自动执行入口
- `scripts/03_prepare_rootfs.sh`：构建独立 initramfs
- `scripts/05_run_qemu_ivshmem.sh`：启动 QEMU
- `scripts/06_extract_records.sh`：从串口日志归档 records
- `driver/day22_ivshmem_stub.c`：day23 继续写 `pci_driver` 的起点

## 最终验收看哪里

看 `records/<RUN_ID>/` 里的这几项：

- `serial.log`
- `lspci-nn.txt`
- `lspci-vv-nn.txt`
- `dmesg-pci.txt`
- `sysfs-pci-devices.txt`
- `qemu.stderr.log`

最终如何判定通过，见 `docs/02_RESULTS_AND_ACCEPTANCE.md`。
