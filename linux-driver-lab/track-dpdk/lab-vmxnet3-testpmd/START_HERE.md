# START_HERE

## 当前 Lab

`lab-vmxnet3-testpmd`

## 当前目标

结合当前测试机环境，完成第一条 DPDK 最小闭环：

```text
Ubuntu 22.04.5 / Linux 6.8.0-110
ens33 管理网保持不动
ens192 / vmxnet3 / 0000:0b:00.0 作为 DPDK 测试口
hugepage + vfio-pci + testpmd + stats 留证
```

## 第 0 步：先确认边界

这台测试机有三张网卡：

| 接口 | 驱动 | 用途 |
|------|------|------|
| `ens33` | e1000 | NAT/SSH 管理，不允许 bind |
| `ens34` | e1000 | 备用 |
| `ens192` | vmxnet3 | DPDK 测试口，可以 bind |

后续脚本默认只使用：

```text
DPDK_IF=ens192
DPDK_PCI=0000:0b:00.0
DPDK_DRIVER=vfio-pci
```

## 第 1 步：只读环境检查

```bash
cd linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd
./scripts/00_check_env.sh
```

重点看：

- `ethtool -i ens192` 是否为 `driver: vmxnet3`
- `lspci -s 0000:0b:00.0` 是否为 `VMXNET3`
- `dpdk-testpmd` 是否存在
- `dpdk-devbind.py` 是否存在
- hugepage 是否已配置
- `vfio-pci` 是否可用

## 第 2 步：配置 hugepage

```bash
sudo ./scripts/01_setup_hugepages.sh
```

默认配置：

```text
HUGEPAGES=1024
HUGEPAGE_MOUNT=/mnt/huge
```

如果内存紧张，可以降低：

```bash
sudo HUGEPAGES=512 ./scripts/01_setup_hugepages.sh
```

## 第 3 步：查看 DPDK 绑定状态

```bash
./scripts/02_bind_vmxnet3.sh status
```

确认 `0000:0b:00.0` 当前驱动状态。

## 第 4 步：绑定 VMXNET3 到 vfio-pci

```bash
sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
```

脚本会：

1. 保存 bind 前状态
2. 尝试 `modprobe vfio-pci`
3. 对 `ens192` 执行 `ip link set ens192 down`
4. 调用 `dpdk-devbind.py -b vfio-pci 0000:0b:00.0`
5. 保存 bind 后状态

## 第 5 步：运行 testpmd

```bash
sudo ./scripts/03_run_testpmd.sh
```

默认运行 20 秒：

```text
TESTPMD_SECONDS=20
TESTPMD_CORES=0-1
TESTPMD_MEM_CHANNELS=4
```

也可以改：

```bash
sudo TESTPMD_SECONDS=60 TESTPMD_CORES=0-3 ./scripts/03_run_testpmd.sh
```

## 第 6 步：收集 stats

```bash
./scripts/04_collect_stats.sh
```

会收集：

- `dpdk-devbind.py --status`
- hugepage 信息
- `lspci -vv -s 0000:0b:00.0`
- `ip -br addr`
- `ethtool -S ens192`（如果仍由内核驱动接管）
- DPDK/testpmd 输出日志

## 第 7 步：生成 review bundle

```bash
./scripts/05_make_review_bundle.sh
```

生成：

```text
reports/lab-vmxnet3-testpmd_exec_board.md
records/<timestamp>-vmxnet3-testpmd/REVIEW_BUNDLE.md
```

## 通过标准

最低通过：

- `00_check_env.sh` 有记录
- hugepage 可用
- `0000:0b:00.0` 能绑定到 DPDK 驱动
- `testpmd` 能启动并输出端口/stats 信息

标准通过：

- records 完整
- 能解释 `ens33` 与 `ens192` 的隔离边界
- 能解释 kernel driver `vmxnet3` 与 DPDK PMD 的切换关系
