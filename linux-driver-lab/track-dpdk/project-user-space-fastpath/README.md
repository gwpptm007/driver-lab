# project-user-space-fastpath

> 所属：`track-dpdk/`

## 一句话定位

把前面四个 DPDK lab 收口成一个**用户态数据面 fastpath 项目原型**。

前面已经完成：

```text
lab-vmxnet3-testpmd       # 真实 VMXNET3 PMD smoke
lab-vhost-user-basic      # vhost-user backend socket smoke
lab-virtio-user-vhost     # virtio-user frontend + vhost-user backend
lab-dpdk-l2-forwarding    # 自己写的最小 L2 forwarding C 程序
```

现在进入：

```text
project-user-space-fastpath
```

## 本项目第一版做什么

`fastpath-lite`：

- DPDK EAL 初始化
- hugepage / mempool / mbuf
- ethdev / RXQ / TXQ 初始化
- poll mode RX/TX loop
- 端口配对转发：`0<->1, 2<->3`
- 单端口 smoke：当前 VMware 测试机可直接跑
- ARP / IPv4 / UDP / non-UDP 分类
- 可选 UDP-only 过滤
- 可选 MAC / IPv4 / UDP port rewrite
- 软件统计 + `rte_eth_stats`

## 推荐入口

```bash
cat START_HERE.md
```

## 当前测试机默认值

```text
管理网卡: ens33 / e1000 / 0000:02:01.0
DPDK 网卡: ens192 / vmxnet3 / 0000:0b:00.0
默认 DPDK driver: uio_pci_generic
```

## 当前验收状态

```text
PASS_SMOKE
```

已经确认：

- `fastpath-lite` 编译成功；
- EAL / mempool / ethdev 初始化成功；
- `ens192/vmxnet3/0000:0b:00.0` 可以由 DPDK 接管；
- port 0 可以启动；
- poll loop 和软件 stats 可以正常输出。

尚未确认：

- 真实 UDP 流量进入 RX；
- `ipv4/udp/non_udp` 计数被真实报文触发；
- rewrite 规则被真实流量命中；
- 双口或 vhost/virtio-user 拓扑下 `tx > 0`。

下一步不要继续堆功能，先进入：

```bash
cd ../project-fastpath-traffic-test
cat START_HERE.md
```

