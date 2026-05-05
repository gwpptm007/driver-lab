# 04_TROUBLESHOOTING

## 1. pkg-config 找不到 libdpdk

```bash
sudo apt update
sudo apt install -y dpdk dpdk-dev libdpdk-dev meson ninja-build pkg-config build-essential
```

## 2. meson/ninja 命令找不到

DPDK 21.11+ 使用 meson+ninja 构建系统：

```bash
sudo apt install -y meson ninja-build
```

## 3. EAL init failed

常见原因：

```text
hugepage 没配置
file-prefix 冲突
没有权限访问 hugepage/uio
PCI 设备没有绑定到 DPDK driver
EAL 参数顺序错误
```

先执行：

```bash
./scripts/00_check_env.sh
sudo ./scripts/02_prepare_vmxnet3.sh
```

## 4. no available DPDK ethdev ports found

EAL 成功但找不到网卡。检查：

```bash
dpdk-devbind.py --status
```

确认 `0000:0b:00.0 drv=uio_pci_generic`，且运行命令里有 `-a 0000:0b:00.0`。

## 5. vfio-pci 失败

VMware Workstation guest 没有完整 IOMMU，`vfio-pci` 失败是预期内问题。当前 lab 默认使用 `uio_pci_generic`。

## 6. RX/TX 一直是 0

当前测试机没有外部发包源，且只有一个 DPDK 口，RX/TX 为 0 不影响 `PASS_SMOKE`。

如果有外部发包源或第二个 DPDK 口，可以验证真实转发。

## 7. SSH 断开风险

脚本内置保护：`DPDK_PCI != MGMT_PCI`，默认不绑定 ens33 到 DPDK driver。

---

## 面试时怎么讲这个项目

可以这样描述：

> 我做了一个 DPDK 用户态 fastpath 原型。前面先用 testpmd 验证 VMXNET3 PMD、hugepage、devbind 和 vhost/virtio-user 环境；然后自己实现了一个 C 语言 fastpath-lite，包含 EAL 初始化、mempool、mbuf、ethdev queue、rx_burst/tx_burst 主循环。数据面里做了 ARP/IPv4/UDP 分类、可选 UDP-only 过滤、MAC/IP/UDP port rewrite 和软件统计。当前在 VMware 单 VMXNET3 口上能完成 smoke，后续接双口或 vhost/virtio-user 后可以验证真实转发。

## 和内核 netdev track 的关系

`netdev` 学的是：

```text
net_device / skb / NAPI / page_pool / ethtool / XDP
```

`track-dpdk` 学的是：

```text
PMD / hugepage / mempool / mbuf / poll mode / user-space forwarding
```

二者对比可以这样说：

- 内核路径强调协议栈集成、通用性、NAPI 调度和 skb 生命周期
- DPDK 路径强调用户态轮询、批量收发、减少内核参与和业务可控 fastpath
- XDP 位于内核早期路径，DPDK 则把网卡队列交给用户态 PMD

## 后续增强方向

- 多队列/RSS
- per-flow rte_hash
- ACL/五元组策略
- 控制面 UNIX socket/JSON 下发
- vhost-user 接 VM
- pktgen/scapy 流量验证
- perf/topdown/PMU 观测