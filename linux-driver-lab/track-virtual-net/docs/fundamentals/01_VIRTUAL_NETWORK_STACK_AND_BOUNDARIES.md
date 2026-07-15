# 01：虚拟网络分层与责任边界

## 一个包跨越的不是“虚拟网络”，而是多个独立子系统

本 track 的重点是连接已有 `netdev` 和 `track-real-driver` 知识。虚拟网络没有消灭网络栈，它把一个物理 NIC 的一部分角色拆给 guest driver、virtio transport、QEMU/vhost、host netdev 和 host bridge。

| 层 | 主要责任 | 关键对象 | 典型证据 |
| --- | --- | --- | --- |
| guest 应用/协议栈 | socket、ARP/ND、IP、TCP/UDP | `skb`、路由、邻居表 | `ip route`、`ip neigh`、`ping` |
| guest driver | TX/RX queue、NAPI、feature negotiation | `virtio_net`、virtqueue | `ethtool -k/-S`、driver log |
| virtio transport | feature/status、共享 ring、通知语义 | PCI virtio device、vring | QEMU device config、guest dmesg |
| backend | 提交/回收 descriptor、事件通知 | QEMU tap backend、`vhost_net` | QEMU 参数、`/dev/vhost-net` |
| host L2 | TAP 收发、FDB 学习、二层 forwarding | tap、bridge、bridge port | `ip -d link`、`bridge link/fdb` |
| host L3/外部网络 | local delivery、routing、NAT、物理 uplink | bridge IP、route、nftables | `ip addr/route`、counter |

## control plane 与 data plane 要分开描述

### Control plane：先把对象接好

以下动作建立关系，但本身不搬运每一帧数据：

- 创建 bridge、TAP，设置 UP 状态、MAC、VLAN、owner；
- 启动 QEMU，绑定 `-netdev` 和 `-device`；
- virtio feature negotiation、virtqueue 创建；
- 开启/关闭 `vhost=on`，配置 `/dev/vhost-net`；
- 配置 guest/host IP、路由、邻居和安全策略。

### Data plane：已建立关系后如何搬帧

数据面只关心已就绪的 queue、descriptor、buffer、TAP 注入/取出和 bridge forwarding。把控制面参数写对不代表数据面可达；反过来，一次偶然 ping 成功也不能替代对控制面状态的记录。

## guest 到 host 与 guest 到 guest 的边界不同

```text
guest -> bridge MAC / host IP
  bridge 进行 local delivery
  -> host IP protocol stack

guest A -> guest B MAC (same bridge)
  bridge FDB forwarding
  -> tap B -> guest B
```

第一条路径最终由 host 协议栈回答 ARP/ICMP；第二条路径在同一二层域内转发，host 不需要把包作为 L3 路由包处理。两者都可能在 host bridge 与 TAP 上可见。

## 两个维度的“位置”

不要只问“包现在在哪”。还要同时问：

1. **拓扑位置**：guest、virtqueue、backend、TAP、bridge port、host local stack、另一个 guest？
2. **所有权位置**：guest driver、backend、TAP file descriptor、bridge forwarding、host socket buffer，谁有权继续修改/释放 buffer？

性能和可靠性问题往往是第二个维度的问题：描述符没有归还、notification 丢失、RX buffer 不足、队列映射错误，都不能仅靠拓扑图解释。

## 特性与能力不是默认承诺

virtio-net、TAP、bridge 和 QEMU 版本都会影响可用能力。文档或脚本必须区分：

| 类别 | 示例 | 应如何表述 |
| --- | --- | --- |
| 已验证能力 | 单队列 TAP + bridge + ping | 给出 records 与命令 |
| 环境探测能力 | `/dev/vhost-net`、multiqueue、offload | 写明探测命令和结果 |
| 设计能力 | RSS、packed ring、vhost-user | 说明前置条件，不能写成已验证结果 |

## Lab 映射

- Lab 1 验证 guest/host 边界和 TAP/bridge 的最小闭环。
- Lab 2 在相同 L2 拓扑上改变 backend；因此适合学习 control plane 不变、data plane 实现可变。
- Lab 3 验证 bridge 作为 L2 switch 的学习和 forwarding，而不是 host routing。
- Project 收集前三者的证据，不额外发明没有被 Lab 验证的性能结论。

## 扩展接口

未来加入 macvtap、OVS、vhost-user、virtio-user、SR-IOV 或 DPDK 时，用同一张表描述：它替换了哪一层、保留哪一层、谁拥有 queue、如何观测。这样新增路径是“替换一个边界”，不是重写全部心智模型。
