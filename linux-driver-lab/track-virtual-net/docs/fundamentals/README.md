# 虚拟网络 Fundamentals：从 virtio 到 tap、bridge 与 vhost

这组文档是 `track-virtual-net` 的知识底座。它不替代现有 Lab 的执行步骤、records 或报告；它负责解释每个 Lab 背后的对象、所有权、边界和可验证结论。

学习目标不是背出一串 QEMU 参数，而是能够回答：一帧 Ethernet 包从 guest `virtio_net` 出发，怎样经过 virtqueue、QEMU 或 `vhost_net`、TAP 与 Linux bridge，到达 host 或另一台 guest；每一段由谁拥有、在哪里排队、用什么证据证明。

## 阅读顺序

| 顺序 | 文档 | 要建立的模型 | 对应实践 |
| --- | --- | --- | --- |
| 00 | [15 分钟心智模型](00_15_MINUTE_MENTAL_MODEL.md) | 六个参与者与两条方向 | 全 track |
| 01 | [虚拟网络分层与边界](01_VIRTUAL_NETWORK_STACK_AND_BOUNDARIES.md) | guest、hypervisor、host 的责任边界 | 全 track |
| 02 | [TUN/TAP 与 host net_device](02_TUN_TAP_AND_HOST_NETDEVICE.md) | TAP 的双向 FD 语义与生命周期 | Lab 1 |
| 03 | [Linux bridge、FDB 与二层转发](03_LINUX_BRIDGE_FDB_VLAN_AND_FORWARDING.md) | 学习、flood、local delivery、VLAN/STP 边界 | Lab 1、Lab 3 |
| 04 | [QEMU netdev 与 virtio-net 前端](04_QEMU_NETDEV_AND_VIRTIO_NET_FRONTEND.md) | `-netdev` 与 `-device` 的组合 | Lab 1、Lab 2 |
| 05 | [Virtqueue、共享内存与事件通知](05_VIRTQUEUE_MEMORY_OWNERSHIP_AND_EVENTFD.md) | descriptor、avail/used、kick/call、所有权 | Lab 2 |
| 06 | [vhost_net 与后端切换](06_VHOST_NET_AND_BACKEND_SWITCHING.md) | `vhost=off/on` 的真正差异 | Lab 2 |
| 07 | [单 guest：TAP/bridge 数据路径](07_SINGLE_GUEST_TAP_BRIDGE_DATA_PATH.md) | guest 到 host 的 ARP/ICMP 路径 | Lab 1 |
| 08 | [双 guest：FDB 学习与二层路径](08_TWO_GUEST_L2_FDB_AND_FORWARDING.md) | unknown-unicast、学习、定向转发 | Lab 3 |
| 09 | [多队列、offload 与性能](09_MULTIQUEUE_OFFLOAD_AND_PERFORMANCE.md) | queue、RSS、GRO/GSO、测量边界 | Lab 2 之后 |
| 10 | [观测与证据设计](10_OBSERVABILITY_AND_EVIDENCE.md) | 状态、抓包、计数器和结论边界 | 所有 Lab |
| 11 | [分层排障手册](11_DEBUGGING_PLAYBOOK.md) | 从 QEMU 参数到 FDB 的定位顺序 | 所有 Lab |
| 12 | [隔离、安全与项目演进](12_SECURITY_ISOLATION_AND_PROJECT_EVOLUTION.md) | 权限、namespace、攻击面、下一阶段扩展 | Project |

## 全程使用的四个不变量

1. **guest 看到的是 virtio-net，不是 host TAP。** TAP 是 host 侧 netdev 与持有其 FD 的用户态/内核后端之间的接口。
2. **bridge 是 host kernel 中的软件二层交换机。** guest-to-guest 的 FDB 命中流量通常不进入 host IP 协议栈，但仍消耗 host CPU。
3. **`vhost=on` 改变的是后端数据路径，不改变 guest 网卡、TAP 名称或 bridge 的二层语义。** 不要用“ping 还能通”替代路径证据。
4. **一个观测点只能证明它所在层发生了什么。** `bridge fdb show` 证明学习结果；`tcpdump` 证明某接口看到了帧；两者都不能单独证明完整 virtqueue 调度时序。

## 可扩展性约定

- 每章只固定概念与边界；具体内核、QEMU、NIC 和虚拟化平台差异放在“版本/能力边界”小节。
- 新实验应复用第 10 章的 evidence 模板：环境、拓扑、命令、原始输出、负结论和清理动作必须可追溯。
- 后续引入 multiqueue、vhost-user、virtio-user、OVS、macvtap、SR-IOV 或 DPDK 时，先把它们放入第 01 章的分层图，再说明它们替换了哪一段，而不是把它们误称为“另一张完全不同的网”。

## 官方资料入口

- Linux 内核：[TUN/TAP](https://docs.kernel.org/networking/tuntap.html)、[Ethernet bridge](https://docs.kernel.org/networking/bridge.html)、[Virtio on Linux](https://docs.kernel.org/driver-api/virtio/virtio.html)
- QEMU：[网络后端与设备模型](https://www.qemu.org/docs/master/system/devices/net.html)
- Virtio 规范：[Virtio 1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/csd01/virtio-v1.2-csd01.pdf)

完成本目录后，再进入 [Lab 1](../../lab-virtio-tap-bridge-path/START_HERE.md)、[Lab 2](../../lab-virtio-vhost-kick-notify/START_HERE.md)、[Lab 3](../../lab-two-guest-bridge-flow/START_HERE.md) 和 [Project](../../project-virtual-net-end-to-end/START_HERE.md)。
