# START_HERE - lab-dpdk-l2-forwarding

## 0. 进入目录

```bash
cd track-dpdk/lab-dpdk-l2-forwarding
```

## 1. 环境检查

```bash
./scripts/00_check_env.sh
```

重点确认：

```text
pkg-config libdpdk: FOUND
meson/ninja/gcc: FOUND
dpdk-devbind: FOUND
DPDK PCI: 0000:0b:00.0
管理网卡 ens33 没有被作为 DPDK_PCI
```

## 2. 编译 l2fwd-lite

```bash
./scripts/01_build_app.sh
```

成功后应生成：

```text
app/build/l2fwd-lite
records/*-dpdk-l2-forwarding/BUILD.log
```

## 3. 准备 VMXNET3 DPDK 口

当前测试机默认使用 `uio_pci_generic`，不是 `vfio-pci`。

```bash
sudo ./scripts/02_prepare_vmxnet3.sh
```

它会做：

```text
配置 hugepage
加载 uio/uio_pci_generic
保护管理网卡 ens33
将 0000:0b:00.0 绑定到 uio_pci_generic
记录 dpdk-devbind 状态
```

## 4. 运行单端口 smoke

```bash
sudo ./scripts/03_run_l2fwd_single_port.sh
```

当前测试机只有一个 DPDK 口，所以日志里出现下面这句是正常的：

```text
notice: only one port is available; running RX/free smoke mode, no L2 peer forwarding.
```

通过重点看：

```text
l2fwd-lite config
port 0 started
available/initialized ports: 1
enter forwarding loop
rte_eth_stats
bye
```

## 5. 可选：vdev null 双端口 smoke

如果当前 DPDK 包包含 `net_null` PMD，可以跑：

```bash
sudo ./scripts/05_run_l2fwd_vdev_null_pair.sh
```

这一步不依赖物理网卡，用来验证“两端口配对初始化”的代码路径。

## 6. 收集与评审

```bash
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

最终看：

```text
records/*-dpdk-l2-forwarding/REVIEW_BUNDLE.md
```

## 7. 下一步

这站通过后进入：

```text
track-dpdk/project-user-space-fastpath
```

后续会把这个 `l2fwd-lite` 演进成更贴近你过往运营商网元经验的用户态 fastpath：

```text
UDP-only filter
ARP/IP/UDP header rewrite
per-port/per-flow stats
control-plane config
records/replay/report
```
