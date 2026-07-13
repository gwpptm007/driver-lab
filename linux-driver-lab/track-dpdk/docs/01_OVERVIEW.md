# 01_OVERVIEW

> 本文是项目路线与环境概览。第一次学习或需要恢复 DPDK 原理时，请先从 [`fundamentals/00_10_MINUTE_MENTAL_MODEL.md`](fundamentals/00_10_MINUTE_MENTAL_MODEL.md) 开始，再回到本文选择项目。

## 目标

从 vmxnet3/testpmd 起步，逐步进入 vhost-user、virtio-user、自写 L2 forwarding C app，最终收成 user-space fastpath / media gateway lite 项目。

承接前面已完成的内容：

- kernel netdev
- real driver
- virtual net

## 当前测试机环境

```text
Guest: Ubuntu 22.04.5 Desktop / Linux 6.8.0-110-generic
管理口: ens33 / e1000 / 192.168.65.135 (不动)
DPDK口: ens192 / vmxnet3 / 0000:0b:00.0 / uio_pci_generic
DPDK版本: 21.11.9
```

## 路线图与状态

### Phase 1: `lab-vmxnet3-testpmd` - PASS

跑通 hugepage、uio、device bind、testpmd、port stats。

### Phase 2: `lab-vhost-user-basic` - PASS

把 vhost_net 视角推进到 DPDK vhost-user socket。

### Phase 3: `lab-virtio-user-vhost` - PASS_WITH_WARN

不依赖完整 VM，理解用户态 virtio frontend 与 vhost backend。

### Phase 4: `lab-dpdk-l2-forwarding` - PASS_SMOKE

实现最小 EAL/mempool/port/queue/rx_burst/tx_burst L2 forwarding。

当前仅证明 C app 初始化、进入 loop、打印 stats；真实双口转发仍需后续流量验证。

### Phase 5: `project-user-space-fastpath` - PASS_PCAP_FUNCTIONAL

整合 vmxnet3/testpmd、vhost-user、virtio-user、L2 forwarding 成 fastpath-lite 原型。

当前已证明：编译、EAL、mempool、vmxnet3 port init、poll loop、分类统计框架。

当前未证明：真实 UDP 流量、rewrite 命中、双口转发。

### Phase 6: `project-fastpath-traffic-test` - NEXT

下一步要做的项目：复用 `fastpath-lite`，补齐真实流量测试。

验收目标：

```text
PASS_TRAFFIC: rx/ipv4/udp 非 0
PASS_REWRITE: rewrite 非 0
PASS_FORWARDING: 双端口或虚拟拓扑 rx/tx 非 0
```

### Phase 7: `project-dpdk-media-gateway-lite` - PASS_PCAP_FUNCTIONAL/FORWARDING/REWRITE

当前已完成项目型骨架和双 vdev smoke，真实 UDP / forwarding / rewrite 后续补。

目标是把 fastpath-lite 项目化为简化媒体网关：规则表、方向、rewrite、drop reason、per-rule stats。

### Phase 8: `project-dpdk-v17-legacy-review` - READY_TO_REVIEW

最后做 DPDK v17 旧项目经验和当前现代 DPDK track 的对照、迁移说明、面试讲法。

## 快速开始

当前下一步：

```bash
cd track-dpdk/project-dpdk-v17-legacy-review
cat START_HERE.md
```

历史 lab 入口：

```bash
cd track-dpdk/lab-vmxnet3-testpmd
cat START_HERE.md
./scripts/00_check_env.sh
```

## VMware Type-2 限制

当前测试机使用 VMware Workstation (Type-2 Hypervisor)，不支持 IOMMU passthrough：

- 使用 `uio_pci_generic` 而非 `vfio-pci`
- DPDK IOVA 模式自动选择 PA（物理地址）
- 单 DPDK 物理口环境下，`rx=0/tx=0` 不能证明真实转发，只能判 `PASS_SMOKE`

## 依赖工具

```bash
# 基础依赖
sudo apt install -y dpdk dpdk-dev libdpdk-dev

# 编译依赖（DPDK 21.11+ 使用 meson）
sudo apt install -y meson ninja-build pkg-config build-essential

# 可选发包工具
sudo apt install -y python3-scapy tcpdump
```
