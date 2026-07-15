# 12：隔离、安全与项目演进

## 虚拟网络实验为什么有安全边界

虚拟网络不是纯本地数据结构。创建 TAP、加入 bridge、打开 `/dev/vhost-net`、启动 QEMU、抓包和修改 bridge/netfilter 都可能影响 host 网络命名空间和其他工作负载。

| 对象 | 风险 | 最小控制 |
| --- | --- | --- |
| TAP FD | 可注入/读取 Ethernet frame | 限制 owner、专用 bridge、关闭后清理 |
| Linux bridge | 把多个端口放入同一二层域 | 不接入管理口/生产 LAN；明确 VLAN/端口关系 |
| QEMU guest | 不受信任的 L2/L3 流量源 | 专用地址段、资源限制、最小权限 |
| vhost device | 允许后端处理 virtqueue/memory 关系 | 仅向需要的进程授权，记录版本/能力 |
| 抓包与 records | MAC、IP、payload、命令可能敏感 | 最小采集、脱敏、避免保存凭据 |

## namespace 不是自动安全边界

network namespace 能隔离接口、路由、bridge 和部分网络状态，但它不自动解决：共享物理 uplink、宿主机权限、QEMU 文件访问、cgroup 资源竞争、错误的 veth/bridge 接线或敏感 records。若用 namespace 扩展实验，需要把 namespace 拓扑和移动接口的命令纳入证据。

## 默认采用专用二层域

基础 Lab 应使用专用 bridge、专用 TAP 名和文档化私有地址段，例如 `br-vnet0` / `tap-vnet-a` / `tap-vnet-b` 与 `192.168.100.0/24`。这不是为了“看起来整齐”，而是减少误接物理管理口、MAC 污染、DHCP 冲突和清理误伤。

## 从当前 Lab 到后续项目的扩展路线

| 下一步 | 新增内容 | 仍应复用的基础模型 |
| --- | --- | --- |
| TAP multiqueue | 多 FD、queue/CPU 映射 | TAP 双向语义、virtqueue 所有权 |
| VLAN/STP | 端口状态、VLAN/FDB 维度 | bridge 学习与 forwarding 条件 |
| vhost-user | 外部 userspace backend、Unix socket、FD 传递 | QEMU 前端/后端分离、virtqueue 语义 |
| virtio-user + DPDK | userspace virtio frontend/PMD | queue/内存/notification 边界 |
| OVS/OVN | flow pipeline、控制面 | “哪一层维护转发表/规则”的问题 |
| SR-IOV/SmartNIC | VF/representor/硬件 offload | endpoint identity、隔离、观测边界 |

扩展原则：先说明它替换了本 track 的哪一段，例如“vhost-user 替换 QEMU/vhost_net backend”，然后保持同一条 evidence 结构；不要因为名称不同就丢掉所有权、队列、L2/L3 边界和清理语义。

## 项目能力地图

完成本 fundamentals 与三个 Lab 后，应能用自己的话描述：

1. guest `virtio_net` 与 host TAP 为什么是两个不同网络对象；
2. virtqueue 的 descriptor/avail/used 如何让双方在共享内存中交接 buffer；
3. QEMU userspace backend 与 `vhost_net` 的控制面相同点、数据面不同点；
4. bridge 如何学习 guest MAC、何时 flood、何时做 local delivery；
5. 为什么 guest-to-guest 不等于 host 路由，且仍会消耗 host kernel CPU；
6. 如何用配置、FDB、抓包、计数器和 workload 组成不夸大的结论；
7. 当以后接入 DPDK vhost-user、AF_XDP 或 SR-IOV 时，哪些前提仍然有效，哪些必须重新验证。

## 复习卡

| 问题 | 简短回答 |
| --- | --- |
| TAP 的 read/write 分别对应什么？ | host 发到 TAP 的帧由后端 read；后端 write 的帧作为 host TAP 入站帧。 |
| bridge FDB 由什么学习？ | 入站帧的源 MAC 与 ingress port。 |
| `vhost=on` 改变什么？ | 主要改变 host backend 数据面实现，不改变 guest virtio 网卡与 bridge 语义。 |
| guest A 到 B 为何不需要 host route？ | 同一 bridge 内按目的 MAC 做 L2 forwarding，非 host L3 路由。 |
| ping 成功能证明什么？ | 当前配置与时间窗口下双向协议可达；不能单独证明队列、性能或所有 feature 正确。 |
| 最重要的排障顺序？ | QEMU/guest device -> TAP/bridge 拓扑 -> ARP/抓包 -> FDB/回程 -> vhost/queue -> 性能。 |

## 收口标准

本目录完成并不意味着“已经做完所有虚拟网络”。它意味着已经建立一套可扩展的推理框架：新增后端、更多队列或更复杂拓扑时，能够明确对象、所有权、数据路径、观测点、能力边界与清理方式。下一步进入 [项目收口](../../project-virtual-net-end-to-end/START_HERE.md)，把这些知识和 Lab records 汇成可展示的工程证据。
