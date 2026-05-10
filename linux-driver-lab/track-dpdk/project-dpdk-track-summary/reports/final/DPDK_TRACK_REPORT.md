# DPDK_TRACK_REPORT

## 1. 总体目标

本 track 的目标是把 Linux 内核网络学习路线延伸到 DPDK 用户态数据面：先掌握 DPDK 环境、PMD、hugepage、uio/vfio，再进入 vhost-user/virtio-user 虚拟化数据面，最后实现自写 C fastpath 和简化媒体网关原型，并将历史 DPDK v17 媒体面经验迁移成现代 DPDK 表达。

## 2. 测试机基线

```text
系统: Ubuntu 22.04.5 Desktop
内核: Linux 6.8.0-110-generic
虚拟化: VMware
管理网卡: ens33 / e1000 / 192.168.65.135
DPDK 网卡: ens192 / vmxnet3 / 0000:0b:00.0
DPDK driver: uio_pci_generic
DPDK 版本: 21.11.9
HugePage: 1024 x 2MB
```

关键约束：

```text
1. ens33 是管理口，不能绑定给 DPDK。
2. ens192 是 DPDK 专用口，可绑定 uio_pci_generic。
3. VMware Workstation 环境下 vfio-pci 可能因 IOMMU 不满足而失败，uio_pci_generic 是当前可用路径。
```

## 3. 阶段完成情况

| 阶段 | 目录 | 状态 | 主要价值 |
|---|---|---|---|
| 1 | `lab-vmxnet3-testpmd` | PASS | DPDK 环境、hugepage、vmxnet3 PMD 接管、testpmd stats |
| 2 | `lab-vhost-user-basic` | PASS | vhost-user backend socket 与 testpmd 最小验证 |
| 3 | `lab-virtio-user-vhost` | PASS_WITH_WARN | virtio-user frontend + vhost-user backend 本机对接 |
| 4 | `lab-dpdk-l2-forwarding` | PASS_SMOKE | 从 testpmd 过渡到自写 DPDK C 程序 |
| 5 | `project-user-space-fastpath` | PASS_SMOKE | fastpath-lite：协议分类、UDP-only、rewrite 框架、软件统计 |
| 6 | `project-fastpath-traffic-test` | READY_TO_TEST | 为真实 UDP 流量、rewrite、统计对照准备测试框架 |
| 7 | `project-dpdk-media-gateway-lite` | PASS_SMOKE | 项目型媒体网关原型，双 vdev smoke 与 UDP-only drop path |
| 8 | `project-dpdk-v17-legacy-review` | PASS_REVIEW | v17 经验复盘、现代 DPDK 对照、面试/简历材料 |

## 4. 能力拆解

### 4.1 DPDK 环境能力

已经覆盖：

```text
hugepage 配置
DPDK 工具检查
dpdk-devbind.py
uio_pci_generic 绑定
testpmd 启动
端口 stats 收集
records/review bundle 留证
```

### 4.2 PMD 与虚拟化数据面能力

已经覆盖：

```text
vmxnet3 PMD
vhost-user backend
virtio-user frontend
vdev/null smoke
本机用户态虚拟链路
```

### 4.3 自写 DPDK C 数据面能力

已经覆盖：

```text
rte_eal_init
rte_pktmbuf_pool_create
rte_eth_dev_configure
rte_eth_rx_queue_setup
rte_eth_tx_queue_setup
rte_eth_rx_burst
rte_eth_tx_burst
rte_eth_stats_get
graceful stop/close
```

### 4.4 Fastpath / Gateway 能力

已经覆盖到 smoke 级别：

```text
Ethernet 解析
ARP / IPv4 / UDP 分类框架
UDP-only 策略
MAC/IP/UDP port rewrite 框架
per-port/per-rule/drop reason stats
rule table
config env
review bundle
```

还需要后续补强：

```text
真实 IPv4/UDP 流量输入
真实 port0->port1 转发
rewrite_hit 非 0
抓包或 stats 对照
```

## 5. 和历史 DPDK v17 项目的关系

历史经验重点是媒体面：

```text
UDP 高速收发
ARP/IP/UDP 重写
KNI 回内核
uio/ko 环境
按网元转发
虚拟化场景媒体面处理
```

当前 modern DPDK track 对应关系：

| 历史 v17 经验 | 当前 track 对应 |
|---|---|
| uio / igb_uio / ko 环境 | uio_pci_generic / devbind / vmxnet3 PMD |
| testpmd / PMD 验证 | lab-vmxnet3-testpmd |
| 虚拟化数据面 | vhost-user / virtio-user |
| UDP 媒体面 | fastpath-lite / media-gateway-lite |
| ARP/IP/UDP rewrite | gateway_rule / gateway_packet / rewrite demo |
| KNI 回内核 | v17 legacy review 中解释历史定位与现代替代方案 |

## 6. 当前不足与后续补测

当前最大不足不是结构，而是真实流量证据不足。

`project-dpdk-media-gateway-lite` 已通过 `PASS_SMOKE`，但还没有达到：

```text
PASS_TRAFFIC
PASS_FORWARDING
PASS_REWRITE
```

后续补测优先级：

```text
P0: pcap PMD 或 vhost/virtio-user 构造真实 UDP 输入
P0: 修正 stats parser，按最后一次累计值判断，而不是把周期累计重复相加
P0: 修正 TX 成功后访问 mbuf 的潜在风险
P1: 补 rewrite_hit 非 0 的 records
P1: 补抓包或 TX stats 对照
```

## 7. 简历价值

这条 track 的简历价值不是“写了一个 l2fwd demo”，而是：

```text
1. 能搭建 DPDK 测试环境并处理 uio/vfio/hugepage/PMD 问题
2. 理解物理 PMD 和用户态虚拟 PMD 的差异
3. 能从 testpmd 过渡到自写 DPDK 数据面程序
4. 能把 fastpath 按解析、规则、改写、统计、配置模块组织起来
5. 能结合旧 DPDK v17 项目经验说明现代化迁移路径
```

## 8. 最终结论

当前 DPDK track 可以作为“用户态数据面学习与项目化作品线”归档。它已经具备完整路径、代码、脚本、记录和面试材料。

对外状态建议写成：

```text
已完成 DPDK 用户态数据面主线，包括 vmxnet3 PMD、vhost-user/virtio-user、自写 l2fwd-lite、fastpath-lite、media-gateway-lite smoke、v17 legacy review；media-gateway-lite 的真实流量 forwarding/rewrite 作为后续补强项。
```
