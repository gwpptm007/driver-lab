# 04：QEMU netdev 与 virtio-net 前端

## QEMU 网络配置有两半，缺一不可

QEMU 把“后端如何连接 host 网络”和“guest 看到什么网卡”拆为两个对象：

```text
-netdev tap,id=net0,...         # host-side backend
-device virtio-net-pci,netdev=net0,...  # guest-side device
```

`id=net0` 是两者的连接点。`-netdev` 只创建/引用一个后端，不会自动让 guest 出现网卡；`-device` 只创建 guest 设备，也不会自动给它接入 host 网络。

## 最小 TAP + virtio-net 组合

```bash
-netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no,vhost=off
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56
```

字段含义：

| 参数 | 作用 | 容易误解的点 |
| --- | --- | --- |
| `ifname=tap-vnet0` | 指定已创建的 host TAP | 它不是 guest 接口名 |
| `script=no,downscript=no` | 不让 QEMU 自动运行网络脚本 | 因此 bridge/TAP 必须提前由实验脚本建立 |
| `id=net0` | backend 标识 | 必须与 `netdev=net0` 一致 |
| `virtio-net-pci` | guest PCI virtio 网卡模型 | guest 由 `virtio_net` 驱动绑定 |
| `mac=...` | guest NIC MAC | 应在 FDB、抓包、guest `ip link` 中一致 |
| `vhost=off/on` | 请求使用普通/内核 vhost tap backend | 不改变 TAP/bridge 拓扑 |

## guest 到底看见什么

guest 内核会枚举一个 virtio PCI 设备，再由 `virtio_net` 创建 `net_device`。接口名字可能是 `eth0`、`ens3` 或由系统命名策略决定；不要把文档中的 `eth0` 当作跨发行版保证。

guest 内的验证顺序：

```bash
ip -br link
ethtool -i <guest-ifname>
ip addr show dev <guest-ifname>
ip neigh show dev <guest-ifname>
```

这些命令分别确认接口存在、驱动绑定、L3 配置和邻居学习。它们不直接告诉你 host TAP 名称；host/guest 应通过 QEMU command line 和 MAC 对应起来。

## virtio feature negotiation 是能力协商，不是固定配置

virtio 设备与驱动在初始化阶段协商 feature bits，例如 checksum、GSO/TSO、multiqueue、控制队列和 event index。实际可用集合取决于 guest 内核、QEMU/device model、backend 和命令行。

写文档或性能结论时：

- 用 `ethtool -k`、QEMU version、guest kernel version 和设备配置记录已验证 feature；
- 不要因为设备类型是 `virtio-net-pci` 就假设所有 offload/multiqueue 功能已启用；
- 不要把 host GRO/GSO 与 guest virtio feature 混为同一个开关，它们处于不同层。

## MAC、IP 与 route 是不同层的配置

| 配置 | 层 | 影响 |
| --- | --- | --- |
| `-device ... mac=` | guest L2 identity | FDB 和 ARP/ND 源 MAC |
| `ip addr add` | guest/host L3 address | 本地地址判断与 ARP/ND target |
| `ip route` | L3 next hop | 决定哪块 guest netdev 发包 |
| bridge port membership | host L2 topology | 决定 TAP 是否进入同一广播域 |

ping 不通时，先判断是哪一层没有建立，不要立刻改 QEMU 参数。

## 与 vhost-user 的区别

`-netdev tap,vhost=on` 使用的是 host kernel `vhost_net`，后端仍面向 TAP。`-netdev vhost-user,...` 则是通过 Unix socket 连接外部 userspace vhost backend，典型用于 DPDK/OVS 等项目。两者都使用 virtio 语义，但控制面、进程边界、内存共享和排障方法不同。

本 track 先只验证 TAP + `vhost_net`。进入 `track-dpdk` 的 virtio-user/vhost-user Lab 时再把外部 userspace backend 作为新的替换层处理。

## 实验验收

- 保存完整 QEMU 网络参数，而不是只保存 `vhost=on` 片段；
- 在 guest 记录接口、MAC、IP、route、neighbor；
- 在 host 记录 TAP/bridge 状态和 FDB；
- 一个实验只改变一个网络变量，避免把设备模型、MAC、IP、vhost 和 bridge 配置同时改掉。

对应实践：[Lab 1 参数](../../lab-virtio-tap-bridge-path/docs/02_TOPOLOGY_AND_EXECUTION.md) 与 [Lab 2 vhost 对照](../../lab-virtio-vhost-kick-notify/docs/02_TOPOLOGY_AND_EXECUTION.md)。
