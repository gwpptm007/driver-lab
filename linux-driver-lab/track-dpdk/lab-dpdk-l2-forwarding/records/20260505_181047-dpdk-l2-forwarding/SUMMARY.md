# SUMMARY

## Lab

lab-dpdk-l2-forwarding

## 测试机环境

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理网卡: ens33 / e1000 / 192.168.65.135
- DPDK 网卡: ens192
- DPDK PCI: 0000:0b:00.0
- 默认 DPDK driver: uio_pci_generic
- DPDK 版本: 21.11.9

## 目标

- [x] 编译 l2fwd-lite (meson + ninja)
- [x] 通过 EAL/mempool/ethdev/queue 初始化
- [x] 进入 rx_burst/tx_burst 数据面循环
- [x] 单端口 smoke（当前仅1个DPDK口）
- [x] 输出软件 stats 与 rte_eth_stats
- [x] 不操作物理网卡（--no-pci 模式）

## 执行命令

```bash
./scripts/00_check_env.sh
sudo apt install -y meson ninja-build  # 首次需安装构建工具
./scripts/01_build_app.sh
sudo ./scripts/02_prepare_vmxnet3.sh
sudo ./scripts/03_run_l2fwd_single_port.sh
./scripts/06_collect_stats.sh
./scripts/07_make_review_bundle.sh
```

## 关键结果

| 项目 | 结果 |
|------|------|
| meson | 0.61.2 |
| ninja | 1.10.1 |
| l2fwd-lite binary | 210KB, ELF 64-bit |
| EAL IOVA 模式 | PA（物理地址） |
| mbuf pool | nb_mbuf=8192, mbuf_cache=250 |
| Port 0 MAC | 00:0C:29:F8:F6:82 |
| Port 初始化 | available/initialized ports: 1 |
| 转发模式 | RX/free smoke (单端口) |
| 软件 stats | rx=0 tx=0 tx_failed=0 |

## 问题

- 无实际问题
- 单端口 smoke 正常：只有一个 VMXNET3，无法测试真正 L2 转发

## 下一步

- Phase: project-user-space-fastpath
- 自写 L2 forwarding 演进，加入 UDP filter、ARP/IP/UDP header rewrite、per-port/per-flow stats