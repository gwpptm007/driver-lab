# 01_GOAL_AND_EXECUTION

## 目标

写出第一个真正的 DPDK C 数据面程序，而不是继续依赖 `testpmd`。

必须覆盖：
```text
rte_eal_init / rte_eal_cleanup
rte_pktmbuf_pool_create
rte_eth_dev_count / RTE_ETH_FOREACH_DEV
rte_eth_dev_configure
rte_eth_rx_queue_setup / rte_eth_tx_queue_setup
rte_eth_dev_start / rte_eth_dev_stop / close
rte_eth_rx_burst / rte_eth_tx_burst
rte_eth_stats_get
```

## 范围

本阶段只做 L2 层最小转发：
```text
收到 mbuf → 解析 Ethernet header → 交换 src/dst MAC → 转发到配对端口 → 统计 rx/tx/drop
```

暂不做：ARP 学习、IP/UDP 解析、flow table、ACL、多 lcore 分发、NUMA 优化、KNI/TAP 回注、控制面配置。

这些放到后续 `project-user-space-fastpath`。

## 测试机环境

```text
Guest OS: Ubuntu 22.04.5 Desktop
Kernel:   Linux 6.8.0-110-generic
User:     wq7
SSH IP:   192.168.65.135
DPDK PCI: 0000:0b:00.0 (ens192 / vmxnet3)
MGMT PCI: 0000:02:01.0 (ens33 / e1000)
```

## 总流程

```text
00_check_env.sh
  ↓
01_build_app.sh
  ↓
02_prepare_vmxnet3.sh
  ↓
03_run_l2fwd_single_port.sh
  ↓
06_collect_stats.sh
  ↓
07_make_review_bundle.sh
```

## C app 内部流程

```text
main
  ↓
rte_eal_init
  ↓
parse app args
  ↓
rte_pktmbuf_pool_create
  ↓
RTE_ETH_FOREACH_DEV
  ↓
init_port
  ├─ rte_eth_dev_info_get
  ├─ rte_eth_dev_configure
  ├─ rte_eth_rx_queue_setup
  ├─ rte_eth_tx_queue_setup
  ├─ rte_eth_dev_start
  └─ rte_eth_promiscuous_enable
  ↓
forwarding_loop
  ├─ rte_eth_rx_burst
  ├─ swap eth src/dst
  ├─ rte_eth_tx_burst 或 no_peer_drop
  └─ periodic software stats
  ↓
rte_eth_stats_get
  ↓
rte_eth_dev_stop / rte_eth_dev_close
  ↓
rte_eal_cleanup
```

## 单端口模式

当前测试机只有一个 DPDK 口，默认 smoke 验证：
```text
port0 RX → 没有 peer port → free mbuf → no_peer_drop++
```

这不是失败，而是本测试机条件下的 smoke 验证。

## 标准执行

```bash
cd track-dpdk/lab-dpdk-l2-forwarding

./scripts/00_check_env.sh
sudo apt install -y meson ninja-build  # 首次需安装
./scripts/01_build_app.sh
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_l2fwd_single_port.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

可选：延长运行时间 `sudo L2FWD_RUN_SECONDS=60 ./scripts/03_run_l2fwd_single_port.sh`