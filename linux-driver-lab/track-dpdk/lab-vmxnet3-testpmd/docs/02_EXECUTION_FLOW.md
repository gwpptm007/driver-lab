# 02_EXECUTION_FLOW

## 总体流程

```text
P0 阅读环境文档
   ↓
P1 只读检查测试机
   ↓
P2 配置 hugepage
   ↓
P3 查看 dpdk-devbind 状态
   ↓
P4 绑定 0000:0b:00.0 到 uio_pci_generic
   ↓
P5 运行 testpmd
   ↓
P6 收集 stats/logs
   ↓
P7 生成 review bundle
```

## P0：阅读环境文档

先读：

```text
track-dpdk/docs/00_ENVIRONMENT_PREPARE.md
```

确认当前测试机是：

```text
ens33  = 管理网，不动
ens192 = vmxnet3 DPDK 测试口
BDF    = 0000:0b:00.0
```

## P1：只读环境检查

```bash
cd linux-driver-lab/track-dpdk/lab-vmxnet3-testpmd
./scripts/00_check_env.sh
```

输出目录类似：

```text
records/20260426_120000-vmxnet3-testpmd/
├── COMMANDS.md
├── ENV_CHECK.txt
├── SUMMARY.md
└── RESULT.md
```

检查重点：

```bash
ip -br addr
ethtool -i ens192
lspci -s 0000:0b:00.0 -nn
dpdk-devbind.py --status
cat /proc/meminfo | grep Huge
```

## P2：配置 hugepage

```bash
sudo ./scripts/01_setup_hugepages.sh
```

默认：

```text
HUGEPAGES=1024
HUGEPAGE_MOUNT=/mnt/huge
```

配置完成后确认：

```bash
grep Huge /proc/meminfo
mount | grep hugetlbfs
```

## P3：查看绑定状态

```bash
./scripts/02_bind_vmxnet3.sh status
```

如果此时 `0000:0b:00.0` 仍由内核接管，通常会看到：

```text
if=ens192 drv=vmxnet3
```

## P4：绑定到 DPDK 驱动

> ⚠️ VMware Workstation 不支持 IOMMU，`vfio-pci` 无法使用。需要使用 `uio_pci_generic` 替代。

```bash
# 1. 加载 uio_pci_generic 驱动
sudo modprobe uio_pci_generic

# 2. 绑定到 uio_pci_generic
sudo DPDK_DRIVER=uio_pci_generic DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
```

验证绑定成功：
```bash
dpdk-devbind.py --status
# 应该看到：0000:0b:00.0 ... drv=uio_pci_generic

ip link show ens192
# 应该显示：Device "ens192" does not exist.
```

为什么要确认变量：

```text
bind 会让 ens192 从 Linux 网络栈消失
虽然 ens192 不是管理口，但仍然属于系统网卡操作
所以脚本要求显式确认，避免误操作
```

## P5：运行 testpmd

```bash
sudo ./scripts/03_run_testpmd.sh
```

默认参数：

```text
-a 0000:0b:00.0
-l 0-1
-n 4
--forward-mode=io
--auto-start
--stats-period=5
```

如果当前机器 CPU 比较多，可以改：

```bash
sudo TESTPMD_CORES=0-3 TESTPMD_SECONDS=60 ./scripts/03_run_testpmd.sh
```

## P6：收集 stats

```bash
./scripts/04_collect_stats.sh
```

输出会补充：

```text
BIND_STATUS.txt
HUGEPAGE_STATUS.txt
PCI_DETAIL.txt
IP_ADDR.txt
ETHTOOL_STATS.txt
DMESG_DPDK_NET.txt
```

## P7：生成 review bundle

```bash
./scripts/05_make_review_bundle.sh
```

输出：

```text
records/<timestamp>-vmxnet3-testpmd/REVIEW_BUNDLE.md
reports/lab-vmxnet3-testpmd_exec_board.md
```

## records 建议结构

```text
records/<timestamp>-vmxnet3-testpmd/
├── SUMMARY.md
├── COMMANDS.md
├── RESULT.md
├── ENV_CHECK.txt
├── HUGEPAGE_SETUP.txt
├── BIND_BEFORE.txt
├── BIND_AFTER.txt
├── TESTPMD.log
├── BIND_STATUS.txt
├── HUGEPAGE_STATUS.txt
├── PCI_DETAIL.txt
├── IP_ADDR.txt
├── ETHTOOL_STATS.txt
├── DMESG_DPDK_NET.txt
└── REVIEW_BUNDLE.md
```
